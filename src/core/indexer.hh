#pragma once

#include "database.hh"
#include "embedder.hh"

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace lazylook
{

struct IndexStats
{
  size_t scanned = 0;
  size_t indexed = 0;
  size_t skipped = 0;
  size_t errors = 0;
};

class Indexer
{
public:
  using ChunkCallback = std::function<void(
      const std::filesystem::path &file_path, size_t chunk_idx, size_t total_chunks)>;
  using ProgressCallback = std::function<void(
      const std::filesystem::path &file_path, size_t scanned, size_t indexed, size_t skipped)>;

  Indexer(Database &db, Embedder &embedder);

  void add_watched_directory(const std::filesystem::path &dir);
  void clear_watched_directories();

  void add_ignore_pattern(std::string_view pattern);
  [[nodiscard]] bool is_ignored(const std::filesystem::path &path) const;

  void set_cancel_token(const std::atomic<bool> *token);

  IndexStats index_all(ProgressCallback progress_cb = nullptr, ChunkCallback chunk_cb = nullptr);
  bool index_file(const std::filesystem::path &path, ChunkCallback chunk_cb = nullptr);
  bool remove_file(const std::filesystem::path &path);

private:
  Database &db_;
  Embedder &embedder_;
  std::vector<std::filesystem::path> watched_dirs_;
  std::vector<std::string> ignore_patterns_;
  const std::atomic<bool> *cancel_token_ = nullptr;
};

} // namespace lazylook
