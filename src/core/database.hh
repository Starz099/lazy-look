#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Forward-declare to keep the header include-light.
namespace lazylook
{
struct TemporalMatch;
}

namespace lazylook
{

struct FileRecord
{
  int64_t id = 0;
  std::string path;
  int64_t size = 0;
  int64_t mtime = 0;
  int64_t indexed_at = 0;
};

struct SearchResult
{
  std::string path;
  std::string snippet;
  float score = 0.0f;
  int64_t file_id = 0;
  int chunk_index = 0;
  int page_number = 0;
  std::string thumbnail_path;
};

struct ChunkRecord
{
  std::string text;
  int page_number = 0;
  std::string thumbnail_path;
};

enum class QueryCategory
{
  All = 0,
  Code,
  Document,
  Image,
  Video,
  Audio,
  Media
};

struct ParsedQuery
{
  QueryCategory category = QueryCategory::All;
  std::string text;
  // Optional mtime lower-bound extracted from a temporal NL hint (epoch secs).
  // Zero means "no time filter".
  int64_t mtime_cutoff = 0;
};

class Database
{
public:
  Database();
  ~Database();

  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;
  Database(Database &&) noexcept;
  Database &operator=(Database &&) noexcept;

  static ParsedQuery parse_query_category(std::string_view query);
  static bool path_matches_category(std::string_view path, QueryCategory category);

  bool open(std::string_view db_path = "");
  void close();
  [[nodiscard]] bool is_open() const;

  std::optional<FileRecord> get_file(std::string_view path);
  bool needs_reindex(std::string_view path, int64_t size, int64_t mtime);

  bool index_file(std::string_view path,
                  int64_t size,
                  int64_t mtime,
                  const std::vector<std::string> &chunks,
                  const std::vector<std::vector<float>> &embeddings);

  std::optional<std::vector<int64_t>> insert_file_chunks(std::string_view path,
                                                         int64_t size,
                                                         int64_t mtime,
                                                         const std::vector<ChunkRecord> &chunks);

  std::optional<std::vector<int64_t>> insert_file_chunks(std::string_view path,
                                                         int64_t size,
                                                         int64_t mtime,
                                                         const std::vector<std::string> &chunks);

  bool insert_chunk_embedding(int64_t chunk_id, const std::vector<float> &embedding);

  bool remove_file(std::string_view path);
  bool remove_directory(std::string_view dir_path);

  std::vector<SearchResult> search_hybrid(const std::vector<float> &query_embedding,
                                          std::string_view query_text,
                                          size_t limit = 20);

  // Returns the indexed text content (OCR or extracted) for the first chunk
  // of the given file path.  Returns an empty string if no text was indexed or
  // the file is not in the database.
  std::string get_file_text(std::string_view path);

  int64_t total_files();
  int64_t total_chunks();

private:
  bool init_schema();

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

std::string default_database_path();

} // namespace lazylook
