#include "config.hh"
#include "database.hh"
#include "embedder.hh"
#include "indexer.hh"
#include "lazylook.hh"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char *argv[])
{
  if (argc >= 3 && std::string_view(argv[1]) == "--index") {
    const std::filesystem::path target_dir = argv[2];
    std::error_code ec;
    if (!std::filesystem::exists(target_dir, ec) ||
        !std::filesystem::is_directory(target_dir, ec)) {
      std::fprintf(stderr, "Directory does not exist: %s\n", argv[2]);
      return 1;
    }

    const std::string model_path = lazylook::Config::resolve_model_path();
    lazylook::Embedder embedder;
    std::printf("Loading embedding model from %s...\n", model_path.c_str());
    if (!embedder.load_model(model_path)) {
      std::fprintf(stderr, "Failed to load model from %s\n", model_path.c_str());
      return 1;
    }

    lazylook::Database db;
    if (!db.open()) {
      std::fprintf(stderr, "Failed to open database\n");
      return 1;
    }

    lazylook::Indexer indexer(db, embedder);
    indexer.add_watched_directory(target_dir);

    std::printf("Indexing directory: %s\n", argv[2]);
    const auto stats =
        indexer.index_all([](const std::filesystem::path &p, size_t, size_t indexed, size_t) {
          std::printf("  [%zu] Indexed: %s\n", indexed, p.filename().string().c_str());
          std::fflush(stdout);
        });

    std::printf("Done. Scanned: %zu, Newly Indexed: %zu, Unchanged: %zu, Errors: %zu\n",
                stats.scanned,
                stats.indexed,
                stats.skipped,
                stats.errors);
    std::printf(
        "Total files in index: %ld (total chunks: %ld)\n", db.total_files(), db.total_chunks());
    return 0;
  }

  if (argc >= 3 && std::string_view(argv[1]) == "--search") {
    const std::string_view query_text = argv[2];

    const std::string model_path = lazylook::Config::resolve_model_path();
    lazylook::Embedder embedder;
    if (!embedder.load_model(model_path)) {
      std::fprintf(stderr, "Failed to load model from %s\n", model_path.c_str());
      return 1;
    }

    lazylook::Database db;
    if (!db.open()) {
      std::fprintf(stderr, "Failed to open database\n");
      return 1;
    }

    const auto query_emb = embedder.embed_query(query_text);
    const auto results = db.search_hybrid(query_emb, query_text, 10);

    if (results.empty()) {
      std::printf("No matching files found.\n");
      return 0;
    }

    std::printf("Found %zu results for \"%s\":\n\n", results.size(), argv[2]);
    for (size_t i = 0; i < results.size(); ++i) {
      std::printf("%zu. [%.4f] %s\n   %s\n\n",
                  i + 1,
                  results[i].score,
                  results[i].path.c_str(),
                  results[i].snippet.c_str());
    }
    return 0;
  }

  if (argc >= 2 && std::string_view(argv[1]) == "--test-db") {
    lazylook::Database db;
    if (!db.open(":memory:")) {
      std::fprintf(stderr, "Failed to open in-memory database\n");
      return 1;
    }

    std::printf("Database schema initialized successfully.\n");

    std::vector<std::string> chunks = {
        "This is an annual financial report detailing revenue, expenses, and profit margins.",
        "A technical specification for an on-device semantic search engine using vectors."};

    std::vector<float> dummy_vec_a(768, 0.01f);
    dummy_vec_a[0] = 0.9f;
    std::vector<float> dummy_vec_b(768, 0.01f);
    dummy_vec_b[1] = 0.9f;

    std::vector<std::vector<float>> embeddings = {dummy_vec_a, dummy_vec_b};

    if (!db.index_file("/test/finance_2026.pdf", 1024, 1700000000, chunks, embeddings)) {
      std::fprintf(stderr, "Failed to index file\n");
      return 1;
    }

    std::printf("Indexed 1 file with %ld chunks.\n", db.total_chunks());
    std::printf("Needs reindex on unchanged file? %s\n",
                db.needs_reindex("/test/finance_2026.pdf", 1024, 1700000000) ? "yes" : "no");
    std::printf("Needs reindex on modified file? %s\n",
                db.needs_reindex("/test/finance_2026.pdf", 2048, 1700000000) ? "yes" : "no");

    auto results = db.search_hybrid(dummy_vec_a, "financial revenue", 10);
    std::printf("Search returned %zu results:\n", results.size());
    for (const auto &r : results) {
      std::printf("  - [%.4f] %s: %s\n", r.score, r.path.c_str(), r.snippet.c_str());
    }

    db.remove_file("/test/finance_2026.pdf");
    std::printf(
        "After removal: total files=%ld, total chunks=%ld\n", db.total_files(), db.total_chunks());
    return 0;
  }

  if (argc >= 2 && std::string_view(argv[1]) == "--test-embed") {
    const std::string model_path = lazylook::Config::resolve_model_path();
    lazylook::Embedder embedder;
    std::printf("Loading embedding model...\n");
    if (!embedder.load_model(model_path)) {
      std::fprintf(stderr, "Failed to load model from %s\n", model_path.c_str());
      return 1;
    }
    auto t0 = std::chrono::steady_clock::now();
    auto emb = embedder.embed_query("machine learning semantic search");
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("Embedded query in %.2f ms, dim=%zu\n", ms, emb.size());
    return 0;
  }

  std::printf(
      "lazylookd %.*s\n", static_cast<int>(lazylook::version.size()), lazylook::version.data());
  std::printf("Usage:\n");
  std::printf("  lazylookd --index <directory>   Index a directory\n");
  std::printf("  lazylookd --search <query>      Search the index with natural language\n");
  std::printf("  lazylookd --test-db             Run database unit test\n");
  return 0;
}
