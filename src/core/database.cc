#include "database.hh"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <sqlite3.h>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

extern "C" {
#include "sqlite-vec.h"
}

namespace lazyfinder
{

namespace
{

std::string clean_snippet_text(std::string_view text)
{
  std::string result;
  result.reserve(text.size());
  bool last_space = true;
  for (char c : text) {
    if (c == '\r' || c == '\t' || c == '\n' || c == ' ') {
      if (!last_space && !result.empty()) {
        result.push_back(' ');
        last_space = true;
      }
    }
    else if (static_cast<unsigned char>(c) > 32) {
      result.push_back(c);
      last_space = false;
    }
  }
  while (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  return result;
}

std::string make_contextual_snippet(std::string_view raw_content,
                                    std::string_view query,
                                    int page_number,
                                    std::string_view ext)
{
  if (raw_content.empty()) {
    return {};
  }

  // Strip prepended filename header if present
  std::string_view content = raw_content;
  const size_t newline_pos = content.find('\n');
  if (newline_pos != std::string_view::npos && newline_pos < 120) {
    content.remove_prefix(newline_pos + 1);
  }

  // Tokenize query terms for context matching
  std::vector<std::string> query_terms;
  std::string curr_term;
  for (char ch : query) {
    if (std::isalnum(static_cast<unsigned char>(ch))) {
      curr_term += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    else if (!curr_term.empty()) {
      if (curr_term.size() >= 2) {
        query_terms.push_back(curr_term);
      }
      curr_term.clear();
    }
  }
  if (!curr_term.empty() && curr_term.size() >= 2) {
    query_terms.push_back(curr_term);
  }

  size_t match_pos = std::string_view::npos;
  size_t match_len = 0;

  if (!query_terms.empty()) {
    std::string lower_content;
    lower_content.reserve(content.size());
    for (char c : content) {
      lower_content.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    for (const auto &term : query_terms) {
      size_t pos = lower_content.find(term);
      if (pos != std::string::npos) {
        if (match_pos == std::string_view::npos || pos < match_pos) {
          match_pos = pos;
          match_len = term.size();
        }
      }
    }
  }

  std::string snippet;
  constexpr size_t window_before = 45;
  constexpr size_t window_after = 110;

  if (match_pos != std::string_view::npos) {
    size_t start = (match_pos > window_before) ? (match_pos - window_before) : 0;
    if (start > 0) {
      while (start < match_pos && !std::isspace(static_cast<unsigned char>(content[start]))) {
        start++;
      }
      if (start < match_pos) {
        start++;
      }
    }

    size_t end = std::min(content.size(), match_pos + match_len + window_after);
    if (end < content.size()) {
      while (end < content.size() && !std::isspace(static_cast<unsigned char>(content[end]))) {
        end++;
      }
    }

    std::string window = clean_snippet_text(content.substr(start, end - start));
    if (start > 0) {
      snippet += "... ";
    }
    snippet += window;
    if (end < content.size()) {
      snippet += " ...";
    }
  }
  else {
    size_t len = std::min<size_t>(content.size(), 160);
    if (len < content.size()) {
      while (len > 0 && !std::isspace(static_cast<unsigned char>(content[len]))) {
        len--;
      }
    }
    snippet = clean_snippet_text(content.substr(0, len > 0 ? len : content.size()));
    if (content.size() > 160) {
      snippet += " ...";
    }
  }

  // Prepend page / slide badge if applicable
  std::string badge;
  if (page_number > 0) {
    if (ext == ".pdf") {
      badge = "Page " + std::to_string(page_number) + " • ";
    }
    else if (ext == ".pptx" || ext == ".odp") {
      badge = "Slide " + std::to_string(page_number) + " • ";
    }
  }

  return badge + snippet;
}

std::string sanitize_fts_query(std::string_view query)
{
  std::vector<std::string> terms;
  size_t i = 0;
  const size_t n = query.size();

  while (i < n) {
    while (i < n && (std::isspace(static_cast<unsigned char>(query[i])) || query[i] == ',' ||
                     query[i] == ';' || query[i] == ':')) {
      i++;
    }
    if (i >= n) {
      break;
    }

    if (query[i] == '"') {
      i++;
      std::string phrase;
      while (i < n && query[i] != '"') {
        char ch = query[i];
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == ' ' || ch == '_' || ch == '-') {
          phrase += ch;
        }
        i++;
      }
      if (i < n && query[i] == '"') {
        i++;
      }
      while (!phrase.empty() && std::isspace(static_cast<unsigned char>(phrase.front()))) {
        phrase.erase(phrase.begin());
      }
      while (!phrase.empty() && std::isspace(static_cast<unsigned char>(phrase.back()))) {
        phrase.pop_back();
      }
      if (!phrase.empty()) {
        terms.push_back("\"" + phrase + "\"");
      }
    }
    else {
      std::string word;
      while (i < n && (std::isalnum(static_cast<unsigned char>(query[i])) || query[i] == '_')) {
        word += query[i];
        i++;
      }
      if (!word.empty()) {
        std::vector<std::string> parts;
        std::string curr;
        for (char ch : word) {
          if (std::isupper(static_cast<unsigned char>(ch)) && !curr.empty() &&
              std::islower(static_cast<unsigned char>(curr.back()))) {
            parts.push_back(curr);
            curr.clear();
          }
          curr += ch;
        }
        if (!curr.empty()) {
          parts.push_back(curr);
        }

        if (parts.size() > 1) {
          std::string inner;
          for (size_t p = 0; p < parts.size(); ++p) {
            if (p > 0) {
              inner += " AND ";
            }
            inner += "\"" + parts[p] + "\"*";
          }
          terms.push_back("(\"" + word + "\"* OR (" + inner + "))");
        }
        else {
          terms.push_back("\"" + word + "\"*");
        }
      }
      else {
        i++;
      }
    }
  }

  std::ostringstream ss;
  for (size_t k = 0; k < terms.size(); ++k) {
    if (k > 0) {
      ss << " AND ";
    }
    ss << terms[k];
  }
  return ss.str();
}

int64_t current_timestamp_secs()
{
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

} // namespace

std::string default_database_path()
{
  const char *xdg_data = std::getenv("XDG_DATA_HOME");
  if (xdg_data && *xdg_data) {
    return (std::filesystem::path(xdg_data) / "lazyfinder" / "index.db").string();
  }

  const char *home = std::getenv("HOME");
  if (home && *home) {
    return (std::filesystem::path(home) / ".local" / "share" / "lazyfinder" / "index.db").string();
  }

  return "index.db";
}

struct Database::Impl
{
  sqlite3 *db = nullptr;
  mutable std::recursive_mutex mutex;
};

Database::Database() : impl_(std::make_unique<Impl>()) {}

Database::~Database()
{
  close();
}

Database::Database(Database &&) noexcept = default;
Database &Database::operator=(Database &&) noexcept = default;

bool Database::open(std::string_view db_path)
{
  if (!impl_) {
    impl_ = std::make_unique<Impl>();
  }
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);

  close();

  const std::string path = db_path.empty() ? default_database_path() : std::string(db_path);

  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

  int rc = sqlite3_open(path.c_str(), &impl_->db);
  if (rc != SQLITE_OK || !impl_->db) {
    close();
    return false;
  }

  rc = sqlite3_vec_init(impl_->db, nullptr, nullptr);
  if (rc != SQLITE_OK) {
    close();
    return false;
  }

  sqlite3_exec(impl_->db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
  sqlite3_exec(impl_->db, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);
  sqlite3_exec(impl_->db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

  return init_schema();
}

void Database::close()
{
  if (!impl_) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  if (impl_->db) {
    sqlite3_close(impl_->db);
    impl_->db = nullptr;
  }
}

bool Database::is_open() const
{
  if (!impl_) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  return impl_->db != nullptr;
}

bool Database::init_schema()
{
  if (!is_open()) {
    return false;
  }

  const char *schema_sql = R"(
    CREATE TABLE IF NOT EXISTS files (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      path TEXT UNIQUE NOT NULL,
      size INTEGER NOT NULL,
      mtime INTEGER NOT NULL,
      indexed_at INTEGER NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_files_path ON files(path);

    CREATE TABLE IF NOT EXISTS chunks (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
      chunk_index INTEGER NOT NULL,
      content TEXT NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_chunks_file_id ON chunks(file_id);

    CREATE VIRTUAL TABLE IF NOT EXISTS vec_chunks USING vec0(
      chunk_id INTEGER PRIMARY KEY,
      embedding float[768] distance_metric=cosine
    );

    CREATE VIRTUAL TABLE IF NOT EXISTS chunks_fts USING fts5(
      content,
      content='chunks',
      content_rowid='id'
    );

    CREATE TRIGGER IF NOT EXISTS chunks_ai AFTER INSERT ON chunks BEGIN
      INSERT INTO chunks_fts(rowid, content) VALUES (new.id, new.content);
    END;

    CREATE TRIGGER IF NOT EXISTS chunks_ad AFTER DELETE ON chunks BEGIN
      INSERT INTO chunks_fts(chunks_fts, rowid, content) VALUES('delete', old.id, old.content);
    END;
  )";

  char *err = nullptr;
  int rc = sqlite3_exec(impl_->db, schema_sql, nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    if (err) {
      sqlite3_free(err);
    }
    return false;
  }

  return true;
}

std::optional<FileRecord> Database::get_file(std::string_view path)
{
  if (!is_open()) {
    return std::nullopt;
  }
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);

  const char *sql = "SELECT id, path, size, mtime, indexed_at FROM files WHERE path = ? LIMIT 1;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }

  sqlite3_bind_text(stmt, 1, path.data(), static_cast<int>(path.size()), SQLITE_STATIC);

  std::optional<FileRecord> result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    FileRecord rec;
    rec.id = sqlite3_column_int64(stmt, 0);
    rec.path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    rec.size = sqlite3_column_int64(stmt, 2);
    rec.mtime = sqlite3_column_int64(stmt, 3);
    rec.indexed_at = sqlite3_column_int64(stmt, 4);
    result = std::move(rec);
  }

  sqlite3_finalize(stmt);
  return result;
}

bool Database::needs_reindex(std::string_view path, int64_t size, int64_t mtime)
{
  auto file = get_file(path);
  if (!file.has_value()) {
    return true;
  }
  return file->size != size || file->mtime != mtime;
}

bool Database::remove_file(std::string_view path)
{
  if (!is_open()) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);

  auto existing = get_file(path);
  if (!existing.has_value()) {
    return true;
  }

  sqlite3_exec(impl_->db, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);

  const char *del_vec_sql =
      "DELETE FROM vec_chunks WHERE chunk_id IN (SELECT id FROM chunks WHERE file_id = ?);";
  sqlite3_stmt *vec_stmt = nullptr;
  if (sqlite3_prepare_v2(impl_->db, del_vec_sql, -1, &vec_stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(vec_stmt, 1, existing->id);
    sqlite3_step(vec_stmt);
    sqlite3_finalize(vec_stmt);
  }

  const char *del_file_sql = "DELETE FROM files WHERE id = ?;";
  sqlite3_stmt *file_stmt = nullptr;
  if (sqlite3_prepare_v2(impl_->db, del_file_sql, -1, &file_stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_int64(file_stmt, 1, existing->id);
    sqlite3_step(file_stmt);
    sqlite3_finalize(file_stmt);
  }

  sqlite3_exec(impl_->db, "COMMIT;", nullptr, nullptr, nullptr);
  return true;
}

bool Database::remove_directory(std::string_view dir_path)
{
  if (!is_open()) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);

  std::string prefix(dir_path);
  while (prefix.size() > 1 && prefix.back() == '/') {
    prefix.pop_back();
  }
  std::string like_pattern = prefix + "/%";

  const char *find_sql = "SELECT path FROM files WHERE path = ? OR path LIKE ?;";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(impl_->db, find_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_text(stmt, 1, prefix.data(), static_cast<int>(prefix.size()), SQLITE_STATIC);
  sqlite3_bind_text(
      stmt, 2, like_pattern.data(), static_cast<int>(like_pattern.size()), SQLITE_STATIC);

  std::vector<std::string> to_remove;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *p = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    if (p) {
      to_remove.emplace_back(p);
    }
  }
  sqlite3_finalize(stmt);

  for (const auto &p : to_remove) {
    remove_file(p);
  }

  return true;
}

std::optional<std::vector<int64_t>> Database::insert_file_chunks(
    std::string_view path, int64_t size, int64_t mtime, const std::vector<std::string> &chunks)
{
  if (!is_open() || chunks.empty()) {
    return std::nullopt;
  }
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);

  remove_file(path);

  sqlite3_exec(impl_->db, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);

  const char *ins_file_sql =
      "INSERT INTO files(path, size, mtime, indexed_at) VALUES (?, ?, ?, ?);";
  sqlite3_stmt *file_stmt = nullptr;
  if (sqlite3_prepare_v2(impl_->db, ins_file_sql, -1, &file_stmt, nullptr) != SQLITE_OK) {
    sqlite3_exec(impl_->db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return std::nullopt;
  }

  sqlite3_bind_text(file_stmt, 1, path.data(), static_cast<int>(path.size()), SQLITE_STATIC);
  sqlite3_bind_int64(file_stmt, 2, size);
  sqlite3_bind_int64(file_stmt, 3, mtime);
  sqlite3_bind_int64(file_stmt, 4, current_timestamp_secs());

  if (sqlite3_step(file_stmt) != SQLITE_DONE) {
    sqlite3_finalize(file_stmt);
    sqlite3_exec(impl_->db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return std::nullopt;
  }
  sqlite3_finalize(file_stmt);

  const int64_t file_id = sqlite3_last_insert_rowid(impl_->db);

  const char *ins_chunk_sql = "INSERT INTO chunks(file_id, chunk_index, content) VALUES (?, ?, ?);";
  sqlite3_stmt *chunk_stmt = nullptr;
  if (sqlite3_prepare_v2(impl_->db, ins_chunk_sql, -1, &chunk_stmt, nullptr) != SQLITE_OK) {
    sqlite3_exec(impl_->db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return std::nullopt;
  }

  std::vector<int64_t> chunk_ids;
  chunk_ids.reserve(chunks.size());

  for (size_t i = 0; i < chunks.size(); ++i) {
    sqlite3_reset(chunk_stmt);
    sqlite3_bind_int64(chunk_stmt, 1, file_id);
    sqlite3_bind_int(chunk_stmt, 2, static_cast<int>(i));
    sqlite3_bind_text(
        chunk_stmt, 3, chunks[i].data(), static_cast<int>(chunks[i].size()), SQLITE_STATIC);

    if (sqlite3_step(chunk_stmt) != SQLITE_DONE) {
      sqlite3_finalize(chunk_stmt);
      sqlite3_exec(impl_->db, "ROLLBACK;", nullptr, nullptr, nullptr);
      return std::nullopt;
    }

    chunk_ids.push_back(sqlite3_last_insert_rowid(impl_->db));
  }

  sqlite3_finalize(chunk_stmt);
  sqlite3_exec(impl_->db, "COMMIT;", nullptr, nullptr, nullptr);

  return chunk_ids;
}

bool Database::insert_chunk_embedding(int64_t chunk_id, const std::vector<float> &embedding)
{
  if (!is_open() || embedding.empty()) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);

  const char *ins_vec_sql = "INSERT OR REPLACE INTO vec_chunks(chunk_id, embedding) VALUES (?, ?);";
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(impl_->db, ins_vec_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_int64(stmt, 1, chunk_id);
  sqlite3_bind_blob(
      stmt, 2, embedding.data(), static_cast<int>(embedding.size() * sizeof(float)), SQLITE_STATIC);

  bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
  sqlite3_finalize(stmt);
  return ok;
}

bool Database::index_file(std::string_view path,
                          int64_t size,
                          int64_t mtime,
                          const std::vector<std::string> &chunks,
                          const std::vector<std::vector<float>> &embeddings)
{
  auto chunk_ids = insert_file_chunks(path, size, mtime, chunks);
  if (!chunk_ids.has_value() || chunk_ids->size() != embeddings.size()) {
    return false;
  }

  for (size_t i = 0; i < embeddings.size(); ++i) {
    if (!embeddings[i].empty()) {
      insert_chunk_embedding((*chunk_ids)[i], embeddings[i]);
    }
  }

  return true;
}

std::vector<SearchResult> Database::search_hybrid(const std::vector<float> &query_embedding,
                                                  std::string_view query_text,
                                                  size_t limit)
{
  if (!is_open()) {
    return {};
  }
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);

  struct Candidate
  {
    std::string path;
    std::string snippet;
    double rrf_score = 0.0;
    int chunk_index = 0;
    std::string best_content;
    int best_rank = 99999;
  };
  std::unordered_map<int64_t, Candidate> candidates;

  const int top_k = static_cast<int>(limit * 3);

  // 1. Dense Vector kNN Search
  if (!query_embedding.empty()) {
    const char *vec_sql = R"(
      SELECT c.file_id, c.chunk_index, c.content, f.path
      FROM vec_chunks v
      JOIN chunks c ON c.id = v.chunk_id
      JOIN files f ON f.id = c.file_id
      WHERE v.embedding MATCH ? AND k = ?
      ORDER BY v.distance;
    )";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db, vec_sql, -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_blob(stmt,
                        1,
                        query_embedding.data(),
                        static_cast<int>(query_embedding.size() * sizeof(float)),
                        SQLITE_STATIC);
      sqlite3_bind_int(stmt, 2, top_k);

      int rank = 1;
      std::unordered_set<int64_t> seen_vec_files;
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t fid = sqlite3_column_int64(stmt, 0);
        int chunk_idx = sqlite3_column_int(stmt, 1);
        const char *content_str = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        const char *path_str = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));

        auto &cand = candidates[fid];
        cand.path = path_str ? path_str : "";
        if (rank < cand.best_rank && content_str) {
          cand.best_rank = rank;
          cand.chunk_index = chunk_idx;
          cand.best_content = content_str;
        }
        if (seen_vec_files.insert(fid).second) {
          cand.rrf_score += 1.0 / (60.0 + rank);
          ++rank;
        }
      }
      sqlite3_finalize(stmt);
    }
  }

  // 2. Sparse Lexical Search (FTS5 BM25)
  const std::string fts_query = sanitize_fts_query(query_text);
  if (!fts_query.empty()) {
    const char *fts_sql = R"(
      SELECT c.file_id, c.chunk_index, c.content, f.path
      FROM chunks_fts fts
      JOIN chunks c ON c.id = fts.rowid
      JOIN files f ON f.id = c.file_id
      WHERE chunks_fts MATCH ?
      ORDER BY rank
      LIMIT ?;
    )";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db, fts_sql, -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, fts_query.c_str(), -1, SQLITE_STATIC);
      sqlite3_bind_int(stmt, 2, top_k);

      int rank = 1;
      std::unordered_set<int64_t> seen_fts_files;
      while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t fid = sqlite3_column_int64(stmt, 0);
        int chunk_idx = sqlite3_column_int(stmt, 1);
        const char *content_str = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        const char *path_str = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));

        auto &cand = candidates[fid];
        cand.path = path_str ? path_str : "";
        if (rank < cand.best_rank && content_str) {
          cand.best_rank = rank;
          cand.chunk_index = chunk_idx;
          cand.best_content = content_str;
        }
        if (seen_fts_files.insert(fid).second) {
          cand.rrf_score += 1.0 / (60.0 + rank);
          ++rank;
        }
      }
      sqlite3_finalize(stmt);
    }
  }

  // 3. Filename matching (boost files whose filename matches query keywords)
  if (!query_text.empty()) {
    std::string clean_q;
    for (const char ch : query_text) {
      if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-') {
        clean_q += ch;
      }
    }
    if (clean_q.size() >= 2) {
      const char *fn_sql = R"(
        SELECT id, path
        FROM files
        WHERE path LIKE ?
        LIMIT ?;
      )";
      sqlite3_stmt *stmt = nullptr;
      if (sqlite3_prepare_v2(impl_->db, fn_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string pattern = "%" + clean_q + "%";
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, top_k);

        int rank = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
          int64_t fid = sqlite3_column_int64(stmt, 0);
          const char *path_str = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
          auto &cand = candidates[fid];
          cand.path = path_str ? path_str : "";
          if (cand.snippet.empty()) {
            cand.snippet = cand.path;
          }
          cand.rrf_score += 1.5 / (60.0 + rank);
          ++rank;
        }
        sqlite3_finalize(stmt);
      }
    }
  }

  std::vector<SearchResult> results;
  results.reserve(candidates.size());

  for (auto &[fid, cand] : candidates) {
    SearchResult res;
    res.file_id = fid;
    res.path = std::move(cand.path);
    res.score = static_cast<float>(cand.rrf_score);
    res.chunk_index = cand.chunk_index;

    std::string ext = std::filesystem::path(res.path).extension().string();
    std::transform(
        ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    int page_num = 0;
    if (ext == ".pdf" || ext == ".pptx" || ext == ".odp") {
      page_num = cand.chunk_index + 1;
    }
    res.page_number = page_num;

    if (!cand.best_content.empty()) {
      res.snippet = make_contextual_snippet(cand.best_content, query_text, page_num, ext);
    }
    else {
      res.snippet = cand.snippet.empty() ? res.path : cand.snippet;
    }

    results.push_back(std::move(res));
  }

  std::sort(results.begin(), results.end(), [](const SearchResult &a, const SearchResult &b) {
    return a.score > b.score;
  });

  if (results.size() > limit) {
    results.resize(limit);
  }

  return results;
}

int64_t Database::total_files()
{
  if (!is_open()) {
    return 0;
  }
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  const char *sql = "SELECT COUNT(*) FROM files;";
  sqlite3_stmt *stmt = nullptr;
  int64_t count = 0;
  if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      count = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }
  return count;
}

int64_t Database::total_chunks()
{
  if (!is_open()) {
    return 0;
  }
  std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
  const char *sql = "SELECT COUNT(*) FROM chunks;";
  sqlite3_stmt *stmt = nullptr;
  int64_t count = 0;
  if (sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      count = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
  }
  return count;
}

} // namespace lazyfinder
