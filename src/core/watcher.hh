#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lazylook
{

class Watcher
{
public:
  using ChangeCallback = std::function<void(const std::vector<std::string> &changed_paths)>;

  Watcher();
  ~Watcher();

  Watcher(const Watcher &) = delete;
  Watcher &operator=(const Watcher &) = delete;

  bool start(const std::vector<std::string> &dirs, ChangeCallback callback);
  void stop();
  void update_directories(const std::vector<std::string> &dirs);
  [[nodiscard]] bool is_running() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace lazylook
