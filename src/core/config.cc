#include "config.hh"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace lazyfinder
{

namespace
{

std::string get_default_home()
{
  const char *home = std::getenv("HOME");
  return (home && *home) ? std::string(home) : "";
}

std::string normalize_dir(std::string_view path)
{
  std::string s(path);
  while (s.size() > 1 && s.back() == '/') {
    s.pop_back();
  }
  return s;
}

} // namespace

Config &Config::instance()
{
  static Config cfg;
  return cfg;
}

Config::Config()
{
  ignore_patterns_ = {"node_modules",
                      "target",
                      "build",
                      "dist",
                      "__pycache__",
                      ".venv",
                      "vendor",
                      ".git",
                      ".cache",
                      ".cargo",
                      ".rustup"};
  load();
}

std::string Config::config_file_path() const
{
  const char *xdg_config = std::getenv("XDG_CONFIG_HOME");
  if (xdg_config && *xdg_config) {
    return (std::filesystem::path(xdg_config) / "lazyfinder" / "config.toml").string();
  }

  const std::string home = get_default_home();
  if (!home.empty()) {
    return (std::filesystem::path(home) / ".config" / "lazyfinder" / "config.toml").string();
  }

  return "lazyfinder.toml";
}

std::string Config::expand_path(std::string_view path)
{
  if (path.empty()) {
    return "";
  }

  if (path == "~") {
    return get_default_home();
  }

  if (path.starts_with("~/")) {
    const std::string home = get_default_home();
    if (!home.empty()) {
      return (std::filesystem::path(home) / path.substr(2)).string();
    }
  }

  return std::string(path);
}

std::string Config::collapse_path(std::string_view path)
{
  const std::string home = get_default_home();
  if (!home.empty() && path.starts_with(home)) {
    if (path.size() == home.size()) {
      return "~";
    }
    if (path[home.size()] == '/') {
      return std::string("~") + std::string(path.substr(home.size()));
    }
  }
  return std::string(path);
}

bool Config::load()
{
  const std::string file_path = config_file_path();
  std::error_code ec;

  if (!std::filesystem::exists(file_path, ec)) {
    const std::string home = get_default_home();
    if (!home.empty()) {
      const auto docs = std::filesystem::path(home) / "Documents";
      if (std::filesystem::exists(docs, ec)) {
        watched_dirs_.push_back("~/Documents");
      }
      const auto downloads = std::filesystem::path(home) / "Downloads";
      if (std::filesystem::exists(downloads, ec)) {
        watched_dirs_.push_back("~/Downloads");
      }
    }
    save();
    return true;
  }

  std::ifstream file(file_path);
  if (!file) {
    return false;
  }

  watched_dirs_.clear();
  std::string line;
  bool in_dirs = false;

  while (std::getline(file, line)) {
    if (line.find("watched_directories") != std::string::npos) {
      if (line.find(']') != std::string::npos) {
        in_dirs = false;
      }
      else {
        in_dirs = true;
      }
      continue;
    }

    if (in_dirs) {
      if (line.find(']') != std::string::npos) {
        in_dirs = false;
        continue;
      }

      auto first_quote = line.find('"');
      auto last_quote = line.rfind('"');
      if (first_quote != std::string::npos && last_quote != std::string::npos &&
          first_quote < last_quote) {
        std::string dir = collapse_path(line.substr(first_quote + 1, last_quote - first_quote - 1));
        if (!dir.empty() &&
            std::find(watched_dirs_.begin(), watched_dirs_.end(), dir) == watched_dirs_.end()) {
          watched_dirs_.push_back(std::move(dir));
        }
      }
    }
  }

  return true;
}

bool Config::save() const
{
  const std::string file_path = config_file_path();
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(file_path).parent_path(), ec);

  std::ofstream file(file_path);
  if (!file) {
    return false;
  }

  file << "# LazyFinder Configuration\n\n";
  file << "[indexer]\n";
  file << "watched_directories = [\n";
  for (size_t i = 0; i < watched_dirs_.size(); ++i) {
    file << "  \"" << watched_dirs_[i] << "\"" << (i + 1 < watched_dirs_.size() ? ",\n" : "\n");
  }
  file << "]\n\n";

  file << "ignore_patterns = [\n";
  for (size_t i = 0; i < ignore_patterns_.size(); ++i) {
    file << "  \"" << ignore_patterns_[i] << "\""
         << (i + 1 < ignore_patterns_.size() ? ",\n" : "\n");
  }
  file << "]\n";

  return true;
}

const std::vector<std::string> &Config::watched_directories() const
{
  return watched_dirs_;
}

bool Config::add_watched_directory(std::string_view dir)
{
  const std::string norm_dir = normalize_dir(dir);
  const std::string collapsed = normalize_dir(collapse_path(norm_dir));
  const std::string expanded = normalize_dir(expand_path(norm_dir));

  if (std::find_if(watched_dirs_.begin(), watched_dirs_.end(), [&](const std::string &d) {
        const std::string norm_d = normalize_dir(d);
        return norm_d == collapsed || norm_d == expanded;
      }) != watched_dirs_.end()) {
    return false;
  }
  watched_dirs_.push_back(collapsed);
  save();
  return true;
}

bool Config::remove_watched_directory(std::string_view dir)
{
  const std::string norm_dir = normalize_dir(dir);
  const std::string collapsed = normalize_dir(collapse_path(norm_dir));
  const std::string expanded = normalize_dir(expand_path(norm_dir));

  auto it = std::remove_if(watched_dirs_.begin(), watched_dirs_.end(), [&](const std::string &d) {
    const std::string norm_d = normalize_dir(d);
    return norm_d == collapsed || norm_d == expanded ||
           normalize_dir(collapse_path(norm_d)) == collapsed ||
           normalize_dir(expand_path(norm_d)) == expanded;
  });
  if (it != watched_dirs_.end()) {
    watched_dirs_.erase(it, watched_dirs_.end());
    save();
    return true;
  }
  return false;
}

const std::vector<std::string> &Config::ignore_patterns() const
{
  return ignore_patterns_;
}

} // namespace lazyfinder
