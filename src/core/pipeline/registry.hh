#pragma once

#include "pipeline/handler.hh"

#include <filesystem>
#include <memory>
#include <vector>

namespace lazylook
{

class HandlerRegistry
{
public:
  HandlerRegistry();
  ~HandlerRegistry();

  HandlerRegistry(const HandlerRegistry &) = delete;
  HandlerRegistry &operator=(const HandlerRegistry &) = delete;
  HandlerRegistry(HandlerRegistry &&) noexcept;
  HandlerRegistry &operator=(HandlerRegistry &&) noexcept;

  void register_handler(std::unique_ptr<IFileHandler> handler);
  [[nodiscard]] IFileHandler *find_handler(const std::filesystem::path &path) const;
  [[nodiscard]] bool is_supported(const std::filesystem::path &path) const;
  [[nodiscard]] FileCategory get_category(const std::filesystem::path &path) const;

  static HandlerRegistry &instance();

private:
  std::vector<std::unique_ptr<IFileHandler>> handlers_;
};

} // namespace lazylook
