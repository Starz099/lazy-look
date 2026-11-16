#include "embedder.hh"

#include <cmath>
#include <llama.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace lazyfinder
{

namespace
{

void ensure_backend_initialized()
{
  static std::once_flag flag;
  std::call_once(flag, []() { llama_backend_init(); });
}

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
};

Embedder::Embedder() : impl_(std::make_unique<Impl>())
{
  ensure_backend_initialized();
}

Embedder::~Embedder()
{
  if (impl_->ctx) {
    llama_free(impl_->ctx);
  }
  if (impl_->model) {
    llama_model_free(impl_->model);
  }
}

Embedder::Embedder(Embedder &&) noexcept = default;
Embedder &Embedder::operator=(Embedder &&) noexcept = default;

bool Embedder::load_model(std::string_view model_path)
{
  if (impl_->ctx) {
    llama_free(impl_->ctx);
    impl_->ctx = nullptr;
  }
  if (impl_->model) {
    llama_model_free(impl_->model);
    impl_->model = nullptr;
  }

  llama_model_params mparams = llama_model_default_params();
  std::string path_str(model_path);
  impl_->model = llama_model_load_from_file(path_str.c_str(), mparams);
  if (!impl_->model) {
    return false;
  }

  impl_->vocab = llama_model_get_vocab(impl_->model);

  llama_context_params cparams = llama_context_default_params();
  cparams.embeddings = true;
  cparams.pooling_type = LLAMA_POOLING_TYPE_UNSPECIFIED;
  cparams.n_ctx = 8192;
  cparams.n_batch = 8192;

  const unsigned int hw_threads = std::thread::hardware_concurrency();
  cparams.n_threads = hw_threads > 0 ? static_cast<int32_t>(hw_threads) : 4;
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

  return true;
}

bool Embedder::is_loaded() const
{
  return impl_->model != nullptr && impl_->ctx != nullptr;
}

int Embedder::embedding_dim() const
{
  return impl_->n_embd;
}

std::vector<float> Embedder::embed_formatted(std::string_view formatted_text)
{
  if (!is_loaded() || formatted_text.empty()) {
    return {};
  }

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

} // namespace lazyfinder
