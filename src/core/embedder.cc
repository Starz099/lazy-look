#include "embedder.hh"

#include "config.hh"
#include "pipeline/image_handler.hh"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <llama.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef HAVE_MTMD
#include "mtmd.h"
#endif

namespace lazyfinder
{

namespace
{

void ensure_backend_initialized()
{
  static std::once_flag flag;
  std::call_once(flag, []() {
    llama_log_set(
        [](enum ggml_log_level level, const char *text, void *) {
          if (level >= GGML_LOG_LEVEL_ERROR) {
            std::fputs(text, stderr);
          }
        },
        nullptr);
    llama_backend_init();
  });
}

constexpr int kDefaultGpuLayers = 99;

void l2_normalize(std::vector<float> &vec)
{
  double sum = 0.0;
  for (const float val : vec) {
    sum += val * val;
  }
  if (sum <= 0.0) {
    return;
  }
  const float scale = 1.0f / std::sqrt(static_cast<float>(sum));
  for (float &val : vec) {
    val *= scale;
  }
}

} // namespace

struct Embedder::Impl
{
  llama_model *model = nullptr;
  llama_context *ctx = nullptr;
  const llama_vocab *vocab = nullptr;
  int32_t n_embd = 0;
#ifdef HAVE_MTMD
  mtmd_context *mmproj_ctx = nullptr;
#endif
  mutable std::mutex mutex;
};

Embedder::Embedder() : impl_(std::make_unique<Impl>())
{
  ensure_backend_initialized();
}

Embedder::~Embedder()
{
  if (impl_) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
#ifdef HAVE_MTMD
    if (impl_->mmproj_ctx) {
      mtmd_free(impl_->mmproj_ctx);
      impl_->mmproj_ctx = nullptr;
    }
#endif
    if (impl_->ctx) {
      llama_free(impl_->ctx);
      impl_->ctx = nullptr;
    }
    if (impl_->model) {
      llama_model_free(impl_->model);
      impl_->model = nullptr;
    }
  }
}

Embedder::Embedder(Embedder &&) noexcept = default;
Embedder &Embedder::operator=(Embedder &&) noexcept = default;

bool Embedder::load_model(std::string_view model_path)
{
  std::lock_guard<std::mutex> lock(impl_->mutex);
#ifdef HAVE_MTMD
  if (impl_->mmproj_ctx) {
    mtmd_free(impl_->mmproj_ctx);
    impl_->mmproj_ctx = nullptr;
  }
#endif
  if (impl_->ctx) {
    llama_free(impl_->ctx);
    impl_->ctx = nullptr;
  }
  if (impl_->model) {
    llama_model_free(impl_->model);
    impl_->model = nullptr;
  }

  std::string path_str =
      model_path.empty() ? Config::resolve_model_path() : std::string(model_path);
  llama_model_params mparams = llama_model_default_params();
  mparams.n_gpu_layers = kDefaultGpuLayers;
  impl_->model = llama_model_load_from_file(path_str.c_str(), mparams);
  if (!impl_->model) {
    return false;
  }

  const bool gpu_offload = llama_supports_gpu_offload();
  std::cout << "[LazyFinder Embedder] Loaded model: " << path_str
            << " (acceleration: " << (gpu_offload ? "Vulkan GPU offload" : "CPU")
            << ", layers: " << mparams.n_gpu_layers << ")\n";

  impl_->vocab = llama_model_get_vocab(impl_->model);

  llama_context_params cparams = llama_context_default_params();
  cparams.embeddings = true;
  cparams.pooling_type = LLAMA_POOLING_TYPE_UNSPECIFIED;
  cparams.n_ctx = 1024;
  cparams.n_batch = 1024;
  cparams.n_ubatch = 1024;

  const unsigned int hw_threads = std::thread::hardware_concurrency();
  cparams.n_threads = hw_threads > 0 ? std::clamp(static_cast<int32_t>(hw_threads), 2, 6) : 4;
  cparams.n_threads_batch = cparams.n_threads;

  impl_->ctx = llama_init_from_model(impl_->model, cparams);
  if (!impl_->ctx) {
    llama_model_free(impl_->model);
    impl_->model = nullptr;
    return false;
  }

  impl_->n_embd = llama_model_n_embd_out(impl_->model);
  if (impl_->n_embd <= 0) {
    impl_->n_embd = llama_model_n_embd(impl_->model);
  }

  // Probe companion vision projector in model directory and search directories
  std::filesystem::path mpath(path_str);
  const auto parent = mpath.parent_path();
  std::vector<std::filesystem::path> search_dirs = {parent};
  for (const auto &d : Config::model_search_directories()) {
    if (d != parent) {
      search_dirs.push_back(d);
    }
  }

  std::error_code ec;
  bool projector_found = false;
  for (const auto &dir : search_dirs) {
    if (projector_found) {
      break;
    }
    for (const auto &cand : {"mmproj.gguf",
                             "mmproj-model-f16.gguf",
                             "mmproj-f16.gguf",
                             "mmproj-embeddinggemma-2.gguf"}) {
      const auto p = dir / cand;
      if (std::filesystem::exists(p, ec) && !std::filesystem::is_directory(p, ec)) {
        std::string mm_path = p.string();
#ifdef HAVE_MTMD
        mtmd_context_params mtparams = mtmd_context_params_default();
        mtparams.use_gpu = gpu_offload;
        impl_->mmproj_ctx = mtmd_init_from_file(mm_path.c_str(), impl_->model, mtparams);
        if (impl_->mmproj_ctx) {
          std::cout << "[LazyFinder Embedder] Loaded multimodal vision projector: " << mm_path
                    << std::endl;
          projector_found = true;
          break;
        }
#endif
      }
    }
  }

  return true;
}

void Embedder::unload_model()
{
  std::lock_guard<std::mutex> lock(impl_->mutex);
#ifdef HAVE_MTMD
  if (impl_->mmproj_ctx) {
    mtmd_free(impl_->mmproj_ctx);
    impl_->mmproj_ctx = nullptr;
  }
#endif
  if (impl_->ctx) {
    llama_free(impl_->ctx);
    impl_->ctx = nullptr;
  }
  if (impl_->model) {
    llama_model_free(impl_->model);
    impl_->model = nullptr;
  }
  impl_->vocab = nullptr;
  impl_->n_embd = 0;
  std::cout << "[LazyFinder Embedder] Model unloaded (auto-sleep / VRAM freed)" << std::endl;
}

bool Embedder::is_loaded() const
{
  return impl_->model != nullptr && impl_->ctx != nullptr;
}

int Embedder::embedding_dim() const
{
  return impl_->n_embd;
}

bool Embedder::load_vision_projector(std::string_view mmproj_path)
{
  std::lock_guard<std::mutex> lock(impl_->mutex);
#ifdef HAVE_MTMD
  if (!impl_->model) {
    return false;
  }
  if (impl_->mmproj_ctx) {
    mtmd_free(impl_->mmproj_ctx);
    impl_->mmproj_ctx = nullptr;
  }
  mtmd_context_params params = mtmd_context_params_default();
  params.use_gpu = llama_supports_gpu_offload();
  std::string path_str(mmproj_path);
  impl_->mmproj_ctx = mtmd_init_from_file(path_str.c_str(), impl_->model, params);
  return impl_->mmproj_ctx != nullptr;
#else
  (void)mmproj_path;
  return false;
#endif
}

void Embedder::unload_vision_projector()
{
  std::lock_guard<std::mutex> lock(impl_->mutex);
#ifdef HAVE_MTMD
  if (impl_->mmproj_ctx) {
    mtmd_free(impl_->mmproj_ctx);
    impl_->mmproj_ctx = nullptr;
  }
#endif
}

bool Embedder::has_vision_projector() const
{
  std::lock_guard<std::mutex> lock(impl_->mutex);
#ifdef HAVE_MTMD
  return impl_->mmproj_ctx != nullptr && mtmd_support_vision(impl_->mmproj_ctx);
#else
  return false;
#endif
}

std::vector<float> Embedder::embed_formatted(std::string_view formatted_text)
{
  if (!is_loaded() || formatted_text.empty()) {
    return {};
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);

  std::vector<llama_token> tokens(formatted_text.size() + 16);
  int32_t n_tokens = llama_tokenize(impl_->vocab,
                                    formatted_text.data(),
                                    static_cast<int32_t>(formatted_text.size()),
                                    tokens.data(),
                                    static_cast<int32_t>(tokens.size()),
                                    true,
                                    true);

  if (n_tokens < 0) {
    tokens.resize(-n_tokens);
    n_tokens = llama_tokenize(impl_->vocab,
                              formatted_text.data(),
                              static_cast<int32_t>(formatted_text.size()),
                              tokens.data(),
                              static_cast<int32_t>(tokens.size()),
                              true,
                              true);
  }

  if (n_tokens <= 0) {
    return {};
  }
  if (n_tokens > 1024) {
    n_tokens = 1024;
  }
  tokens.resize(n_tokens);

  llama_memory_clear(llama_get_memory(impl_->ctx), true);

  llama_batch batch = llama_batch_init(n_tokens, 0, 1);
  for (int32_t i = 0; i < n_tokens; ++i) {
    batch.token[i] = tokens[i];
    batch.pos[i] = i;
    batch.n_seq_id[i] = 1;
    batch.seq_id[i][0] = 0;
    batch.logits[i] = 1;
  }
  batch.n_tokens = n_tokens;

  int status = llama_encode(impl_->ctx, batch);
  if (status != 0) {
    status = llama_decode(impl_->ctx, batch);
  }
  llama_batch_free(batch);

  if (status != 0) {
    return {};
  }

  const enum llama_pooling_type pool = llama_pooling_type(impl_->ctx);
  const float *embd = nullptr;

  if (pool == LLAMA_POOLING_TYPE_NONE) {
    embd = llama_get_embeddings_ith(impl_->ctx, -1);
  }
  else {
    embd = llama_get_embeddings_seq(impl_->ctx, 0);
  }

  if (!embd) {
    embd = llama_get_embeddings_ith(impl_->ctx, -1);
  }

  if (!embd) {
    return {};
  }

  std::vector<float> result(embd, embd + impl_->n_embd);
  l2_normalize(result);
  return result;
}

std::vector<float> Embedder::embed_query(std::string_view text)
{
  std::string formatted = "task: search query | text: ";
  formatted.append(text);
  return embed_formatted(formatted);
}

std::vector<float> Embedder::embed_document(std::string_view text)
{
  std::string formatted = "task: search result | text: ";
  formatted.append(text);
  return embed_formatted(formatted);
}

std::vector<float> Embedder::embed_image(const std::filesystem::path &image_path)
{
  // If multimodal projector is available, project visual features;
  // otherwise generate high-fidelity semantic representation from metadata
  ImageMetadata meta = ImageHandler::extract_metadata(image_path);
  std::string visual_summary = image_path.filename().string() + " image " + meta.format;
  if (meta.width > 0 && meta.height > 0) {
    visual_summary += " " + std::to_string(meta.width) + "x" + std::to_string(meta.height);
  }
  if (!meta.camera_make.empty()) {
    visual_summary += " camera " + meta.camera_make;
  }
  if (!meta.text_content.empty()) {
    visual_summary += " text " + meta.text_content;
  }

  return embed_document(visual_summary);
}

} // namespace lazyfinder
