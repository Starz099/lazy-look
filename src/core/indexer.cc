#include "indexer.hh"

#include "pipeline/registry.hh"

#include <fnmatch.h>
#include <iostream>
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

constexpr size_t kProgressLogInterval = 100;

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
      if (name == pattern || fnmatch(pattern.c_str(), name.c_str(), 0) == 0) {
        return true;
      }
    }
  }
  return false;
}

void Indexer::set_cancel_token(const std::atomic<bool> *token)
{
  cancel_token_ = token;
}

bool Indexer::index_file(const std::filesystem::path &path, ChunkCallback chunk_cb)
{
  if (cancel_token_ && cancel_token_->load()) {
    return false;
  }

  auto *handler = HandlerRegistry::instance().find_handler(path);
  if (!handler || is_ignored(path)) {
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

  auto processed = handler->process(path);
  if (processed.chunks.empty()) {
    processed.chunks.push_back(ProcessedChunk{path.filename().string(), 0, {}});
  }

  std::vector<ChunkRecord> chunk_records;
  chunk_records.reserve(processed.chunks.size());
  for (const auto &chunk : processed.chunks) {
    chunk_records.push_back(ChunkRecord{chunk.text, chunk.page_or_timestamp, chunk.thumbnail_path});
  }

  // Insert file record and text chunks immediately for instant lexical search
  auto chunk_ids = db_.insert_file_chunks(path_str, size, mtime, chunk_records);
  if (!chunk_ids.has_value() || chunk_ids->size() != processed.chunks.size()) {
    return false;
  }

  if (chunk_cb) {
    chunk_cb(path, 0, processed.chunks.size());
  }

  // Generate vector embeddings for each chunk
  for (size_t i = 0; i < processed.chunks.size(); ++i) {
    if (cancel_token_ && cancel_token_->load()) {
      return true; // Preserved in lexical index; vector search can resume next sync
    }
    std::vector<float> emb;
    if (!processed.chunks[i].custom_embedding.empty()) {
      emb = processed.chunks[i].custom_embedding;
    }
    else {
      emb = embedder_.embed_document(processed.chunks[i].text);
    }
    if (!emb.empty()) {
      db_.insert_chunk_embedding((*chunk_ids)[i], emb);
    }
    if (chunk_cb) {
      chunk_cb(path, i + 1, processed.chunks.size());
    }
  }

  return true;
}

bool Indexer::remove_file(const std::filesystem::path &path)
{
  return db_.remove_file(path.string());
}

IndexStats Indexer::index_all(ProgressCallback progress_cb, ChunkCallback chunk_cb)
{
  IndexStats stats;

  for (const auto &dir : watched_dirs_) {
    if (cancel_token_ && cancel_token_->load()) {
      break;
    }

    std::error_code ec;
    auto iter = std::filesystem::recursive_directory_iterator(
        dir, std::filesystem::directory_options::skip_permission_denied, ec);

    for (auto it = std::filesystem::begin(iter); it != std::filesystem::end(iter);
         it.increment(ec)) {
      if (cancel_token_ && cancel_token_->load()) {
        break;
      }

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

      if (!HandlerRegistry::instance().is_supported(entry.path()) || is_ignored(entry.path())) {
        continue;
      }

      struct stat st;
      if (stat(entry.path().c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        if (!db_.needs_reindex(entry.path().string(), st.st_size, st.st_mtime)) {
          stats.skipped++;
          if (progress_cb && (stats.scanned % kProgressLogInterval == 0 || stats.skipped == 1)) {
            progress_cb(entry.path(), stats.scanned, stats.indexed, stats.skipped);
          }
          continue;
        }
      }

      if (index_file(entry.path(), chunk_cb)) {
        stats.indexed++;
        if (progress_cb) {
          progress_cb(entry.path(), stats.scanned, stats.indexed, stats.skipped);
        }
      }
      else {
        stats.errors++;
        std::cerr << "[Sync] Warning: Failed to index " << entry.path().filename().string()
                  << std::endl;
      }
    }
  }

  return stats;
}

} // namespace lazyfinder
