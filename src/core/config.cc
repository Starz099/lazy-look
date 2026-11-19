#include "config.hh"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace lazylook
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
    const auto look_path = std::filesystem::path(xdg_config) / "lazylook" / "config.toml";
    const auto finder_path = std::filesystem::path(xdg_config) / "lazyfinder" / "config.toml";
    if (!std::filesystem::exists(look_path) && std::filesystem::exists(finder_path)) {
      return finder_path.string();
    }
    return look_path.string();
  }

  const std::string home = get_default_home();
  if (!home.empty()) {
    const auto look_path = std::filesystem::path(home) / ".config" / "lazylook" / "config.toml";
    const auto finder_path = std::filesystem::path(home) / ".config" / "lazyfinder" / "config.toml";
    if (!std::filesystem::exists(look_path) && std::filesystem::exists(finder_path)) {
      return finder_path.string();
    }
    return look_path.string();
  }

  if (!std::filesystem::exists("lazylook.toml") && std::filesystem::exists("lazyfinder.toml")) {
    return "lazyfinder.toml";
  }
  return "lazylook.toml";
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

std::vector<std::filesystem::path> Config::model_search_directories()
{
  std::vector<std::filesystem::path> dirs;
  std::error_code ec;

  // Environment variable override
  const char *env_dir = std::getenv("LAZYLOOK_MODELS_DIR");
  if (!env_dir || !*env_dir) {
    env_dir = std::getenv("LAZYFINDER_MODELS_DIR");
  }
  if (env_dir && *env_dir) {
    dirs.emplace_back(env_dir);
  }

  // Application-relative directory for AppImage and portable bundles
  if (std::filesystem::exists("/proc/self/exe", ec)) {
    const auto exe_path = std::filesystem::canonical("/proc/self/exe", ec);
    if (!ec && !exe_path.empty()) {
      const auto exe_dir = exe_path.parent_path();
      dirs.push_back(exe_dir.parent_path() / "share" / "lazylook" / "models");
      dirs.push_back(exe_dir.parent_path() / "share" / "lazyfinder" / "models");
      dirs.push_back(exe_dir / "models");
    }
  }

  // User XDG data directory
  const char *xdg_data = std::getenv("XDG_DATA_HOME");
  if (xdg_data && *xdg_data) {
    dirs.push_back(std::filesystem::path(xdg_data) / "lazylook" / "models");
    dirs.push_back(std::filesystem::path(xdg_data) / "lazyfinder" / "models");
  }
  else {
    const std::string home = get_default_home();
    if (!home.empty()) {
      dirs.push_back(std::filesystem::path(home) / ".local" / "share" / "lazylook" / "models");
      dirs.push_back(std::filesystem::path(home) / ".local" / "share" / "lazyfinder" / "models");
    }
  }

  // System-wide directories
  dirs.emplace_back("/usr/share/lazylook/models");
  dirs.emplace_back("/usr/share/lazyfinder/models");
  dirs.emplace_back("/usr/local/share/lazylook/models");
  dirs.emplace_back("/usr/local/share/lazyfinder/models");

  // Working directory fallbacks
  dirs.push_back(std::filesystem::current_path() / "models");
  dirs.push_back(std::filesystem::current_path());

  return dirs;
}

std::string Config::default_user_model_path(std::string_view filename)
{
  const std::string fname = filename.empty() ? "embeddinggemma-2-Q8_0.gguf" : std::string(filename);
  const char *xdg_data = std::getenv("XDG_DATA_HOME");
  if (xdg_data && *xdg_data) {
    const auto look_path = std::filesystem::path(xdg_data) / "lazylook" / "models" / fname;
    const auto finder_path = std::filesystem::path(xdg_data) / "lazyfinder" / "models" / fname;
    if (!std::filesystem::exists(look_path) && std::filesystem::exists(finder_path)) {
      return finder_path.string();
    }
    return look_path.string();
  }
  const std::string home = get_default_home();
  if (!home.empty()) {
    const auto look_path =
        std::filesystem::path(home) / ".local" / "share" / "lazylook" / "models" / fname;
    const auto finder_path =
        std::filesystem::path(home) / ".local" / "share" / "lazyfinder" / "models" / fname;
    if (!std::filesystem::exists(look_path) && std::filesystem::exists(finder_path)) {
      return finder_path.string();
    }
    return look_path.string();
  }
  return fname;
}

std::string Config::resolve_model_path(std::string_view preferred_model)
{
  std::error_code ec;

  // If a direct absolute/relative path was passed and exists, return it
  if (!preferred_model.empty()) {
    std::filesystem::path p(preferred_model);
    if (std::filesystem::exists(p, ec) && !std::filesystem::is_directory(p, ec)) {
      return std::filesystem::canonical(p, ec).string();
    }
  }

  const auto search_dirs = model_search_directories();

  if (!preferred_model.empty()) {
    for (const auto &dir : search_dirs) {
      if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        continue;
      }
      const auto specific = dir / preferred_model;
      if (std::filesystem::exists(specific, ec) && !std::filesystem::is_directory(specific, ec)) {
        return specific.string();
      }
    }
    return "";
  }

  static const std::vector<std::string> candidates = {"embeddinggemma-2-Q8_0.gguf",
                                                      "embeddinggemma-2-BF16.gguf",
                                                      "embeddinggemma2-q8_0.gguf",
                                                      "embeddinggemma-2.gguf"};

  for (const auto &dir : search_dirs) {
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
      continue;
    }

    for (const auto &cand : candidates) {
      const auto cand_path = dir / cand;
      if (std::filesystem::exists(cand_path, ec) && !std::filesystem::is_directory(cand_path, ec)) {
        return cand_path.string();
      }
    }
  }

  return default_user_model_path();
}

bool Config::is_model_installed(std::string_view preferred_model)
{
  std::error_code ec;
  const std::string path = resolve_model_path(preferred_model);
  return !path.empty() && std::filesystem::exists(path, ec) &&
         !std::filesystem::is_directory(path, ec);
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

    if (line.find("global_shortcut") != std::string::npos) {
      auto first_quote = line.find('"');
      auto last_quote = line.rfind('"');
      if (first_quote != std::string::npos && last_quote != std::string::npos &&
          first_quote < last_quote) {
        global_shortcut_ = line.substr(first_quote + 1, last_quote - first_quote - 1);
      }
      continue;
    }

    if (line.find("auto_sync") != std::string::npos) {
      auto eq = line.find('=');
      if (eq != std::string::npos) {
        auto val = line.substr(eq + 1);
        auto pos = val.find_first_not_of(" \t");
        if (pos != std::string::npos) {
          auto sub = val.substr(pos, 4);
          auto sub_true = val.substr(pos, 5);
          auto sub_clean = val.substr(pos);
          auto false_pos = sub_clean.find("false");
          auto true_pos = sub_clean.find("true");
          if (false_pos != std::string::npos) {
            auto true_earlier = sub_clean.find("true");
            if (true_earlier == std::string::npos || false_pos < true_earlier) {
              auto sub_false = sub_clean.substr(false_pos, 5);
              if (sub_false == "false")
                auto_sync_ = false;
            }
          }
          if (true_pos != std::string::npos && auto_sync_) {
            auto_sync_ = true;
          }
        }
      }
      continue;
    }

    if (line.find("launch_on_startup") != std::string::npos) {
      if (line.find("true") != std::string::npos) {
        launch_on_startup_ = true;
      }
      else if (line.find("false") != std::string::npos) {
        launch_on_startup_ = false;
      }
      continue;
    }

    if (line.find("memory_saver") != std::string::npos) {
      if (line.find("false") != std::string::npos) {
        memory_saver_ = false;
      }
      else if (line.find("true") != std::string::npos) {
        memory_saver_ = true;
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

  if (launch_on_startup_) {
    set_autostart_desktop_file(true);
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

  file << "# LazyLook Configuration\n\n";
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
  file << "]\n\n";

  file << "[system]\n";
  file << "global_shortcut = \"" << global_shortcut_ << "\"\n";
  file << "auto_sync = " << (auto_sync_ ? "true" : "false") << "\n";
  file << "launch_on_startup = " << (launch_on_startup_ ? "true" : "false") << "\n";
  file << "memory_saver = " << (memory_saver_ ? "true" : "false") << "\n";

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

const std::string &Config::global_shortcut() const
{
  return global_shortcut_;
}

void Config::set_global_shortcut(std::string_view shortcut)
{
  global_shortcut_ = std::string(shortcut);
  register_gnome_shortcut(global_shortcut_);
  save();
}

bool Config::auto_sync() const
{
  return auto_sync_;
}

void Config::set_auto_sync(bool enabled)
{
  auto_sync_ = enabled;
  save();
}

bool Config::launch_on_startup() const
{
  return launch_on_startup_;
}

void Config::set_launch_on_startup(bool enabled)
{
  launch_on_startup_ = enabled;
  set_autostart_desktop_file(enabled);
  save();
}

bool Config::memory_saver() const
{
  return memory_saver_;
}

void Config::set_memory_saver(bool enabled)
{
  memory_saver_ = enabled;
  save();
}

bool Config::register_gnome_shortcut(std::string_view shortcut)
{
  std::string sc(shortcut);
  if (sc.empty()) {
    sc = "<Control><Shift>space";
  }

  // Check if gsettings is available
  if (std::system("which gsettings >/dev/null 2>&1") != 0) {
    return false;
  }

  const std::string path =
      "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom-lazylook/";
  const std::string legacy_path =
      "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom-lazyfinder/";

  std::string cmd =
      "python3 -c \"\n"
      "import subprocess\n"
      "path = '" +
      path +
      "'\n"
      "legacy_path = '" +
      legacy_path +
      "'\n"
      "cur = subprocess.run(['gsettings', 'get', 'org.gnome.settings-daemon.plugins.media-keys', "
      "'custom-keybindings'], capture_output=True, text=True).stdout.strip()\n"
      "try:\n"
      "    items = eval(cur) if cur.startswith('[') else []\n"
      "except:\n"
      "    items = []\n"
      "if legacy_path in items:\n"
      "    items.remove(legacy_path)\n"
      "if path not in items:\n"
      "    items.append(path)\n"
      "s_list = '[' + ', '.join(f'\\'{x}\\' ' for x in items) + ']'\n"
      "subprocess.run(['gsettings', 'set', 'org.gnome.settings-daemon.plugins.media-keys', "
      "'custom-keybindings', s_list])\n"
      "subprocess.run(['gsettings', 'set', "
      "f'org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:{path}', 'name', "
      "'LazyLook'])\n"
      "subprocess.run(['gsettings', 'set', "
      "f'org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:{path}', 'command', "
      "'sh -c \\\"PATH=\\$HOME/.local/bin:\\$HOME/.cargo/bin:/usr/local/bin:/usr/bin:\\$PATH exec "
      "lazylook --toggle\\\"'])\n"
      "subprocess.run(['gsettings', 'set', "
      "f'org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:{path}', 'binding', '" +
      sc +
      "'])\n"
      "\" >/dev/null 2>&1";

  int ret = std::system(cmd.c_str());
  return ret == 0;
}

bool Config::unregister_gnome_shortcut()
{
  if (std::system("which gsettings >/dev/null 2>&1") != 0) {
    return false;
  }

  const std::string path =
      "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom-lazylook/";
  const std::string legacy_path =
      "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom-lazyfinder/";

  std::string cmd =
      "python3 -c \"\n"
      "import subprocess\n"
      "path = '" +
      path +
      "'\n"
      "legacy_path = '" +
      legacy_path +
      "'\n"
      "cur = subprocess.run(['gsettings', 'get', 'org.gnome.settings-daemon.plugins.media-keys', "
      "'custom-keybindings'], capture_output=True, text=True).stdout.strip()\n"
      "try:\n"
      "    items = eval(cur) if cur.startswith('[') else []\n"
      "except:\n"
      "    items = []\n"
      "updated = False\n"
      "if path in items:\n"
      "    items.remove(path)\n"
      "    updated = True\n"
      "if legacy_path in items:\n"
      "    items.remove(legacy_path)\n"
      "    updated = True\n"
      "if updated:\n"
      "    s_list = '[' + ', '.join(f'\\'{x}\\' ' for x in items) + ']'\n"
      "    subprocess.run(['gsettings', 'set', 'org.gnome.settings-daemon.plugins.media-keys', "
      "'custom-keybindings', s_list])\n"
      "subprocess.run(['gsettings', 'reset-recursively', "
      "f'org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:{path}'])\n"
      "subprocess.run(['gsettings', 'reset-recursively', "
      "f'org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:{legacy_path}'])\n"
      "\" >/dev/null 2>&1";

  int ret = std::system(cmd.c_str());
  return ret == 0;
}

bool Config::set_autostart_desktop_file(bool enabled)
{
  const std::string home = get_default_home();
  if (home.empty()) {
    return false;
  }
  const auto autostart_dir = std::filesystem::path(home) / ".config" / "autostart";
  const auto desktop_path = autostart_dir / "lazylook.desktop";
  const auto legacy_desktop_path = autostart_dir / "lazyfinder.desktop";

  std::error_code ec;
  std::filesystem::remove(legacy_desktop_path, ec);

  if (!enabled) {
    std::filesystem::remove(desktop_path, ec);
    return true;
  }

  std::filesystem::create_directories(autostart_dir, ec);
  std::ofstream file(desktop_path);
  if (!file) {
    return false;
  }
  file << "[Desktop Entry]\n"
       << "Version=1.0\n"
       << "Type=Application\n"
       << "Name=LazyLook\n"
       << "GenericName=Desktop Search\n"
       << "Comment=Instant AI & Hybrid Desktop Search\n"
       << "Exec=sh -c \"PATH=$HOME/.local/bin:$HOME/.cargo/bin:/usr/local/bin:/usr/bin:$PATH exec "
          "lazylook --daemon\"\n"
       << "Icon=lazylook\n"
       << "Terminal=false\n"
       << "Categories=Utility;Filesystem;\n"
       << "Hidden=false\n"
       << "NoDisplay=false\n"
       << "X-GNOME-Autostart-enabled=true\n";
  return true;
}

} // namespace lazylook
