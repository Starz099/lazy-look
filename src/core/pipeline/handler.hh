#pragma once

#include "pipeline/item.hh"

#include <filesystem>
#include <string_view>

namespace lazyfinder
{

class IFileHandler
{
public:
  virtual ~IFileHandler() = default;

  [[nodiscard]] virtual FileCategory category() const = 0;
  [[nodiscard]] virtual std::string_view name() const = 0;
  [[nodiscard]] virtual bool supports(const std::filesystem::path &path) const = 0;
  [[nodiscard]] virtual ProcessedFile process(const std::filesystem::path &path) = 0;
};

} // namespace lazyfinder
