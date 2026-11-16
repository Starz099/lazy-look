#include "database.hh"
#include "embedder.hh"
#include "lazyfinder.hh"

#include <cstdio>
#include <string_view>
#include <vector>

int main(int argc, char *argv[])
{
  if (argc >= 4 && std::string_view(argv[1]) == "--test-embed") {
    const std::string_view model_path = argv[2];
    const std::string_view query_text = argv[3];

    lazyfinder::Embedder embedder;
    std::printf("Loading model from: %s\n", argv[2]);
    if (!embedder.load_model(model_path)) {
      std::fprintf(stderr, "Failed to load model\n");
      return 1;
    }

    std::printf("Model loaded. Embedding dim: %d\n", embedder.embedding_dim());
    const auto vec = embedder.embed_query(query_text);
    if (vec.empty()) {
      std::fprintf(stderr, "Failed to generate embedding\n");
      return 1;
    }

    std::printf("Generated embedding vector of size %zu\n", vec.size());
    std::printf("Sample values: [%.5f, %.5f, %.5f, %.5f, ...]\n", vec[0], vec[1], vec[2], vec[3]);
    return 0;
  }

  if (argc >= 2 && std::string_view(argv[1]) == "--test-db") {
    lazyfinder::Database db;
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

  if (argc >= 4 && std::string_view(argv[1]) == "--test-hybrid") {
    const std::string_view model_path = argv[2];
    const std::string_view query_text = argv[3];

    lazyfinder::Embedder embedder;
    std::printf("Loading model: %s\n", argv[2]);
    if (!embedder.load_model(model_path)) {
      std::fprintf(stderr, "Failed to load model\n");
      return 1;
    }

    lazyfinder::Database db;
    if (!db.open(":memory:")) {
      std::fprintf(stderr, "Failed to open in-memory database\n");
      return 1;
    }

    struct SampleDoc
    {
      std::string path;
      std::string content;
    };

    std::vector<SampleDoc> docs = {
        {"/docs/quarterly_budget.txt",
         "The Q3 budget forecast includes marketing expenses, employee salaries, and vendor "
         "contracts totaling two million dollars."},
        {"/code/http_router.rs",
         "pub struct Router { routes: HashMap<String, Box<dyn Handler>> } impl Router { pub fn "
         "add_route(&mut self, path: &str) { } }"},
        {"/notes/vacation_plans.md",
         "Pack sunscreen, beach towels, and camera gear for the summer holiday trip to Greece."}};

    std::printf("Embedding and indexing %zu sample documents...\n", docs.size());
    for (const auto &doc : docs) {
      auto emb = embedder.embed_document(doc.content);
      db.index_file(
          doc.path, static_cast<int64_t>(doc.content.size()), 1700000000, {doc.content}, {emb});
    }

    std::printf("Running hybrid search for query: \"%s\"\n", argv[3]);
    auto query_emb = embedder.embed_query(query_text);
    auto results = db.search_hybrid(query_emb, query_text, 10);

    std::printf("\nRanked Results (Combined Vector + Lexical RRF):\n");
    for (size_t i = 0; i < results.size(); ++i) {
      std::printf("  %zu. Score: %.4f | File: %s\n     Snippet: %s\n",
                  i + 1,
                  results[i].score,
                  results[i].path.c_str(),
                  results[i].snippet.c_str());
    }
    return 0;
  }

  std::printf("lazyfinderd %.*s\n",
              static_cast<int>(lazyfinder::version.size()),
              lazyfinder::version.data());
  return 0;
}
