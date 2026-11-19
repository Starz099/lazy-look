#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lazyfinder
{

class Embedder
{
public:
  Embedder();
  ~Embedder();

  Embedder(const Embedder &) = delete;
  Embedder &operator=(const Embedder &) = delete;
  Embedder(Embedder &&) noexcept;
  Embedder &operator=(Embedder &&) noexcept;

  bool load_model(std::string_view model_path);
  void unload_model();
  [[nodiscard]] bool is_loaded() const;
  [[nodiscard]] int embedding_dim() const;

  std::vector<float> embed_query(std::string_view text);
  std::vector<float> embed_document(std::string_view text);

private:
  std::vector<float> embed_formatted(std::string_view formatted_text);

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace lazyfinder
