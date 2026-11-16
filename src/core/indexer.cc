#include "indexer.hh"

#include "extractor.hh"

#include <sys/stat.h>

namespace lazyfinder
{

namespace
{

const std::vector<std::string> &default_ignores()
{
  static const std::vector<std::string> ignores = {"node_modules",
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
  return ignores;
}

} // namespace

Indexer::Indexer(Database &db, Embedder &embedder)
    : db_(db), embedder_(embedder), ignore_patterns_(default_ignores())
{
}

void Indexer::add_watched_directory(const std::filesystem::path &dir)
{
  std::error_code ec;
  if (std::filesystem::exists(dir, ec) && std::filesystem::is_directory(dir, ec)) {
    watched_dirs_.push_back(std::filesystem::canonical(dir, ec));
  }
}

void Indexer::clear_watched_directories()
{
  watched_dirs_.clear();
}

void Indexer::add_ignore_pattern(std::string_view pattern)
{
  ignore_patterns_.emplace_back(pattern);
}

bool Indexer::is_ignored(const std::filesystem::path &path) const
{
  for (const auto &part : path) {
    const std::string name = part.string();
    if (name.empty()) {
      continue;
    }

    if (name.size() > 1 && name.front() == '.' && name != "..") {
      return true;
    }

    for (const auto &pattern : ignore_patterns_) {
      if (name == pattern) {
        return true;
      }
    }
  }
  return false;
}

bool Indexer::index_file(const std::filesystem::path &path)
{
  if (!Extractor::is_supported(path) || is_ignored(path)) {
    return false;
  }

  struct stat st;
  if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
    return false;
  }

  const int64_t size = st.st_size;
  const int64_t mtime = st.st_mtime;
  const std::string path_str = path.string();

  if (!db_.needs_reindex(path_str, size, mtime)) {
    return false;
  }

  const auto chunks = Extractor::extract_chunks(path);
  if (chunks.empty()) {
    return false;
  }

  std::vector<std::vector<float>> embeddings;
  embeddings.reserve(chunks.size());

  for (const auto &chunk : chunks) {
    auto emb = embedder_.embed_document(chunk);
    if (emb.empty()) {
      return false;
    }
    embeddings.push_back(std::move(emb));
  }

  return db_.index_file(path_str, size, mtime, chunks, embeddings);
}

bool Indexer::remove_file(const std::filesystem::path &path)
{
  return db_.remove_file(path.string());
}

IndexStats Indexer::index_all()
{
  IndexStats stats;

  for (const auto &dir : watched_dirs_) {
    std::error_code ec;
    auto iter = std::filesystem::recursive_directory_iterator(
        dir, std::filesystem::directory_options::skip_permission_denied, ec);

    for (auto it = std::filesystem::begin(iter); it != std::filesystem::end(iter);
         it.increment(ec)) {
      if (ec) {
        stats.errors++;
        continue;
      }

      const auto &entry = *it;
      if (entry.is_directory(ec)) {
        if (is_ignored(entry.path())) {
          it.disable_recursion_pending();
        }
        continue;
      }

      if (!entry.is_regular_file(ec)) {
        continue;
      }

      stats.scanned++;

      if (!Extractor::is_supported(entry.path()) || is_ignored(entry.path())) {
        continue;
      }

      struct stat st;
      if (stat(entry.path().c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        if (!db_.needs_reindex(entry.path().string(), st.st_size, st.st_mtime)) {
          stats.skipped++;
          continue;
        }
      }

      if (index_file(entry.path())) {
        stats.indexed++;
      }
      else {
        stats.errors++;
      }
    }
  }

  return stats;
}

} // namespace lazyfinder
