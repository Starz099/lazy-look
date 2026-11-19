#include "config.hh"
#include "embedder.hh"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace
{

#define TEST_ASSERT(cond, msg)                                                                     \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      std::cerr << "Assertion failed: " << (msg) << " at " << __FILE__ << ":" << __LINE__          \
                << std::endl;                                                                      \
      std::exit(1);                                                                                \
    }                                                                                              \
  } while (false)

float dot_product(const std::vector<float> &a, const std::vector<float> &b)
{
  TEST_ASSERT(a.size() == b.size() && !a.empty(), "Vectors must have matching non-zero size");
  float sum = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    sum += a[i] * b[i];
  }
  return sum;
}

float vector_norm(const std::vector<float> &v)
{
  return std::sqrt(dot_product(v, v));
}

void test_uninitialized_state()
{
  std::cout << "[Test] Embedder Uninitialized State..." << std::endl;
  lazylook::Embedder embedder;

  TEST_ASSERT(!embedder.is_loaded(), "Uninitialized embedder is not loaded");
  TEST_ASSERT(embedder.embedding_dim() == 0, "Uninitialized embedder dim is 0");

  auto empty_res = embedder.embed_query("");
  TEST_ASSERT(empty_res.empty(), "Empty query on uninitialized embedder returns empty");

  auto query_res = embedder.embed_query("some test text");
  TEST_ASSERT(query_res.empty(), "Query on uninitialized embedder returns empty safely");

  std::cout << "  ✓ Uninitialized safety passed." << std::endl;
}

void test_model_loading_and_inference()
{
  std::cout << "[Test] Embedder Model Loading and L2 Normalization..." << std::endl;

  std::string model_path = lazylook::Config::resolve_model_path();
  if (model_path.empty() || !std::filesystem::exists(model_path)) {
    std::cout << "  (Skipping live model test: model weights not found at resolved path)"
              << std::endl;
    return;
  }

  lazylook::Embedder embedder;
  bool loaded = embedder.load_model(model_path);
  TEST_ASSERT(loaded, "Failed to load model from resolved path");
  TEST_ASSERT(embedder.is_loaded(), "Embedder reports loaded state");
  TEST_ASSERT(embedder.embedding_dim() == 768, "Embedding dimension must be 768 for Gemma-2");

  // Generate embedding for a test query
  auto vec = embedder.embed_query("quarterly financial report and revenue statistics");
  TEST_ASSERT(vec.size() == 768, "Vector dimension matches embedding dim");

  // Verify unit normalization
  float norm = vector_norm(vec);
  TEST_ASSERT(std::abs(norm - 1.0f) < 1e-3f, "Vector must be L2 normalized to unit length");

  std::cout << "  ✓ Model loading and L2 normalization passed (dim=" << vec.size()
            << ", norm=" << norm << ")." << std::endl;

  // Test Semantic Similarity:
  // dot(AI, Tech) must be significantly higher than dot(AI, Food)
  auto vec_ai =
      embedder.embed_query("machine learning neural network models and artificial intelligence");
  auto vec_tech = embedder.embed_query(
      "software development deep learning algorithms and computer programming");
  auto vec_food =
      embedder.embed_query("baking fresh sourdough bread with flour, water and yeast in oven");

  TEST_ASSERT(vec_ai.size() == 768 && vec_tech.size() == 768 && vec_food.size() == 768,
              "All vectors generated");

  float sim_related = dot_product(vec_ai, vec_tech);
  float sim_unrelated = dot_product(vec_ai, vec_food);

  std::cout << "  Cosine similarity (AI <-> Tech): " << sim_related << std::endl;
  std::cout << "  Cosine similarity (AI <-> Food): " << sim_unrelated << std::endl;

  TEST_ASSERT(sim_related > sim_unrelated,
              "Related semantic vectors must have higher cosine similarity than unrelated");
  TEST_ASSERT(sim_related - sim_unrelated > 0.1f, "Semantic margin must be substantial (> 0.10)");

  std::cout << "  ✓ Semantic vector mathematics verified." << std::endl;

  // Test Auto-Sleep / Unload
  embedder.unload_model();
  TEST_ASSERT(!embedder.is_loaded(), "Embedder is marked unloaded after unload_model()");
  TEST_ASSERT(embedder.embedding_dim() == 0, "Embedding dimension reset to 0");

  auto post_unload = embedder.embed_query("test query after unload");
  TEST_ASSERT(post_unload.empty(), "Query after unload returns empty safely");

  std::cout << "  ✓ Auto-sleep unloading passed." << std::endl;
}

} // namespace

int main()
{
  std::cout << "========================================" << std::endl;
  std::cout << "LazyLook Embedder & Vector Math Tests   " << std::endl;
  std::cout << "========================================" << std::endl;

  test_uninitialized_state();
  test_model_loading_and_inference();

  std::cout << "========================================" << std::endl;
  std::cout << "ALL EMBEDDER TESTS PASSED SUCCESSFULLY! " << std::endl;
  std::cout << "========================================" << std::endl;

  return 0;
}
