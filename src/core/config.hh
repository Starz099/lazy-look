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

  [[nodiscard]] const std::string &global_shortcut() const;
  void set_global_shortcut(std::string_view shortcut);

  [[nodiscard]] bool auto_sync() const;
  void set_auto_sync(bool enabled);

  [[nodiscard]] bool launch_on_startup() const;
  void set_launch_on_startup(bool enabled);

  [[nodiscard]] bool memory_saver() const;
  void set_memory_saver(bool enabled);

  static bool register_gnome_shortcut(std::string_view shortcut);
  static bool unregister_gnome_shortcut();
  static bool set_autostart_desktop_file(bool enabled);

  static std::string expand_path(std::string_view path);
  static std::string collapse_path(std::string_view path);

private:
  Config();
  std::string config_file_path() const;

  std::vector<std::string> watched_dirs_;
  std::vector<std::string> ignore_patterns_;
  std::string global_shortcut_ = "<Control><Shift>space";
  bool auto_sync_ = true;
  bool launch_on_startup_ = false;
  bool memory_saver_ = true;
};

} // namespace lazyfinder
