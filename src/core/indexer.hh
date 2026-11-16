#pragma once

#include "database.hh"
#include "embedder.hh"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lazyfinder
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
  Indexer(Database &db, Embedder &embedder);

  void add_watched_directory(const std::filesystem::path &dir);
  void clear_watched_directories();

  void add_ignore_pattern(std::string_view pattern);
  [[nodiscard]] bool is_ignored(const std::filesystem::path &path) const;

  IndexStats index_all();
  bool index_file(const std::filesystem::path &path);
  bool remove_file(const std::filesystem::path &path);

private:
  Database &db_;
  Embedder &embedder_;
  std::vector<std::filesystem::path> watched_dirs_;
  std::vector<std::string> ignore_patterns_;
};

} // namespace lazyfinder
