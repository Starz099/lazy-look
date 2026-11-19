#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace lazyfinder
{

class Config
{
public:
  static Config &instance();

  Config(const Config &) = delete;
  Config &operator=(const Config &) = delete;

  bool load();
  bool save() const;

  const std::vector<std::string> &watched_directories() const;
  bool add_watched_directory(std::string_view dir);
  bool remove_watched_directory(std::string_view dir);

  const std::vector<std::string> &ignore_patterns() const;

  static std::string expand_path(std::string_view path);
  static std::string collapse_path(std::string_view path);

private:
  Config();
  std::string config_file_path() const;

  std::vector<std::string> watched_dirs_;
  std::vector<std::string> ignore_patterns_;
};

} // namespace lazyfinder
