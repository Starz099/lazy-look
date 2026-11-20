#include "watcher.hh"

#include "config.hh"

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

namespace lazylook
{

struct Watcher::Impl
{
  int inotify_fd = -1;
  int stop_fd = -1;
  std::atomic<bool> running{false};
  std::thread worker_thread;
  std::mutex map_mutex;

  std::unordered_map<int, std::string> wd_to_path;
  std::unordered_map<std::string, int> path_to_wd;
  ChangeCallback callback;

  void add_watch_recursive(const std::filesystem::path &root);
  void remove_all_watches();
  void run();
};

void Watcher::Impl::add_watch_recursive(const std::filesystem::path &root)
{
  std::error_code ec;
  if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
    return;
  }

  const auto &ignore = Config::instance().ignore_patterns();

  for (std::filesystem::recursive_directory_iterator
           it(root, std::filesystem::directory_options::skip_permission_denied, ec),
       end;
       it != end;
       it.increment(ec)) {
    if (ec) {
      continue;
    }

    const std::string name = it->path().filename().string();
    if (name.starts_with('.')) {
      it.disable_recursion_pending();
      continue;
    }
    bool is_ignored = false;
    for (const auto &ign : ignore) {
      if (name == ign) {
        is_ignored = true;
        break;
      }
    }
    if (is_ignored) {
      it.disable_recursion_pending();
      continue;
    }

    if (it->is_directory(ec)) {
      const std::string dir_str = it->path().string();
      std::lock_guard<std::mutex> lock(map_mutex);
      if (path_to_wd.find(dir_str) == path_to_wd.end()) {
        int wd =
            inotify_add_watch(inotify_fd,
                              dir_str.c_str(),
                              IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM | IN_CREATE);
        if (wd >= 0) {
          wd_to_path[wd] = dir_str;
          path_to_wd[dir_str] = wd;
        }
      }
    }
  }

  // Also watch the root directory itself
  const std::string root_str = root.string();
  std::lock_guard<std::mutex> lock(map_mutex);
  if (path_to_wd.find(root_str) == path_to_wd.end()) {
    int wd =
        inotify_add_watch(inotify_fd,
                          root_str.c_str(),
                          IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM | IN_CREATE);
    if (wd >= 0) {
      wd_to_path[wd] = root_str;
      path_to_wd[root_str] = wd;
    }
  }
}

void Watcher::Impl::remove_all_watches()
{
  std::lock_guard<std::mutex> lock(map_mutex);
  for (const auto &[wd, path] : wd_to_path) {
    if (inotify_fd >= 0) {
      inotify_rm_watch(inotify_fd, wd);
    }
  }
  wd_to_path.clear();
  path_to_wd.clear();
}

void Watcher::Impl::run()
{
  alignas(struct inotify_event) char buffer[4096 * sizeof(struct inotify_event)];
  std::unordered_set<std::string> pending_changes;
  auto debounce_deadline = std::chrono::steady_clock::time_point::max();
  constexpr auto debounce_duration = std::chrono::seconds(3);
  constexpr int64_t kMinPollTimeoutMs = 10;
  constexpr nfds_t kPollFdCount = 2;

  struct pollfd fds[kPollFdCount];
  fds[0].fd = inotify_fd;
  fds[0].events = POLLIN;
  fds[1].fd = stop_fd;
  fds[1].events = POLLIN;

  while (running.load()) {
    int timeout_ms = -1;
    if (!pending_changes.empty()) {
      auto now = std::chrono::steady_clock::now();
      if (now >= debounce_deadline) {
        timeout_ms = 0;
      }
      else {
        auto rem = std::chrono::duration_cast<std::chrono::milliseconds>(debounce_deadline - now);
        timeout_ms = static_cast<int>(std::max<int64_t>(kMinPollTimeoutMs, rem.count()));
      }
    }

    int ret = poll(fds, kPollFdCount, timeout_ms);
    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }

    // Check stop signal
    if (fds[1].revents & POLLIN) {
      break;
    }

    // Check inotify events
    if (fds[0].revents & POLLIN) {
      ssize_t len = read(inotify_fd, buffer, sizeof(buffer));
      if (len > 0) {
        ssize_t i = 0;
        while (i < len) {
          auto *event = reinterpret_cast<struct inotify_event *>(&buffer[i]);
          if (event->len > 0 && event->name[0] != '.') {
            std::string parent_dir;
            {
              std::lock_guard<std::mutex> lock(map_mutex);
              auto it = wd_to_path.find(event->wd);
              if (it != wd_to_path.end()) {
                parent_dir = it->second;
              }
            }

            if (!parent_dir.empty()) {
              std::string full_path = parent_dir + "/" + event->name;

              // If a new directory was created, watch it recursively
              if (event->mask & IN_ISDIR) {
                if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                  add_watch_recursive(full_path);
                }
              }
              else {
                pending_changes.insert(full_path);
                debounce_deadline = std::chrono::steady_clock::now() + debounce_duration;
              }
            }
          }
          i += static_cast<ssize_t>(sizeof(struct inotify_event) + event->len);
        }
      }
    }

    // Check debounce timeout
    if (!pending_changes.empty()) {
      auto now = std::chrono::steady_clock::now();
      if (now >= debounce_deadline) {
        std::vector<std::string> paths(pending_changes.begin(), pending_changes.end());
        pending_changes.clear();
        debounce_deadline = std::chrono::steady_clock::time_point::max();

        if (callback && running.load()) {
          callback(paths);
        }
      }
    }
  }
}

Watcher::Watcher() : impl_(std::make_unique<Impl>()) {}

Watcher::~Watcher()
{
  stop();
}

bool Watcher::start(const std::vector<std::string> &dirs, ChangeCallback callback)
{
  stop();

  impl_->inotify_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
  if (impl_->inotify_fd < 0) {
    return false;
  }

  impl_->stop_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (impl_->stop_fd < 0) {
    close(impl_->inotify_fd);
    impl_->inotify_fd = -1;
    return false;
  }

  impl_->callback = std::move(callback);
  impl_->running.store(true);

  for (const auto &d : dirs) {
    impl_->add_watch_recursive(Config::expand_path(d));
  }

  impl_->worker_thread = std::thread([this]() { impl_->run(); });
  std::cout << "[LazyLook Watcher] Inotify file watcher active across " << impl_->path_to_wd.size()
            << " directories." << std::endl;
  return true;
}

void Watcher::stop()
{
  if (!impl_->running.load()) {
    return;
  }
  impl_->running.store(false);

  if (impl_->stop_fd >= 0) {
    uint64_t val = 1;
    [[maybe_unused]] ssize_t written = write(impl_->stop_fd, &val, sizeof(val));
  }

  if (impl_->worker_thread.joinable()) {
    impl_->worker_thread.join();
  }

  impl_->remove_all_watches();

  if (impl_->inotify_fd >= 0) {
    close(impl_->inotify_fd);
    impl_->inotify_fd = -1;
  }
  if (impl_->stop_fd >= 0) {
    close(impl_->stop_fd);
    impl_->stop_fd = -1;
  }
  std::cout << "[LazyLook Watcher] Inotify file watcher stopped." << std::endl;
}

void Watcher::update_directories(const std::vector<std::string> &dirs)
{
  if (!impl_->running.load()) {
    return;
  }
  impl_->remove_all_watches();
  for (const auto &d : dirs) {
    impl_->add_watch_recursive(Config::expand_path(d));
  }
  std::cout << "[LazyLook Watcher] Updated watched paths: " << impl_->path_to_wd.size()
            << " directories monitored." << std::endl;
}

bool Watcher::is_running() const
{
  return impl_->running.load();
}

} // namespace lazylook
