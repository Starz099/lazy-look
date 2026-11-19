#include "database.hh"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{

#define TEST_ASSERT(cond, msg)                                                                     \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      std::cerr << "Assertion failed: " << (msg) << " at " << __FILE__ << ":" << __LINE__          \
                << std::endl;                                                                      \
      std::exit(1);                                                                                \
    }                                                                                              \
  } while (false)

std::filesystem::path temp_db_path()
{
  return std::filesystem::temp_directory_path() /
         ("lazyfinder_db_test_" + std::to_string(std::rand()) + ".db");
}

std::vector<float> make_dummy_vector(size_t dim, float seed)
{
  std::vector<float> vec(dim, 0.0f);
  for (size_t i = 0; i < dim; ++i) {
    vec[i] = seed + static_cast<float>(i) * 0.001f;
  }
  return vec;
}

} // namespace

void test_database_crud()
{
  std::cout << "[Test] Database CRUD and Stage 1/2 Ingestion..." << std::endl;

  const auto db_path = temp_db_path();
  lazyfinder::Database db;

  TEST_ASSERT(db.open(db_path.string()), "Database open and init schema");
  TEST_ASSERT(db.is_open(), "Database is marked open");
  TEST_ASSERT(db.total_files() == 0, "Initial files count is 0");
  TEST_ASSERT(db.total_chunks() == 0, "Initial chunks count is 0");

  // Stage 1: Insert file and chunks
  const std::string file_path = "/test/documents/financial_report.pdf";
  const std::vector<std::string> chunks = {
      "financial_report.pdf\nQuarterly Revenue: 50 Million USD",
      "financial_report.pdf\nOperating Expenses and Net Income"};

  auto chunk_ids = db.insert_file_chunks(file_path, 1024, 1700000000, chunks);
  TEST_ASSERT(chunk_ids.has_value(), "Insert file chunks returns ids");
  TEST_ASSERT(chunk_ids->size() == 2, "Returned 2 chunk ids");
  TEST_ASSERT(db.total_files() == 1, "File count is 1");
  TEST_ASSERT(db.total_chunks() == 2, "Chunk count is 2");

  // Reindex check
  TEST_ASSERT(!db.needs_reindex(file_path, 1024, 1700000000),
              "Same size and mtime does not need reindex");
  TEST_ASSERT(db.needs_reindex(file_path, 2048, 1700000000), "Different size needs reindex");
  TEST_ASSERT(db.needs_reindex(file_path, 1024, 1700000050), "Different mtime needs reindex");

  // Stage 2: Insert embeddings (768-dim)
  auto vec1 = make_dummy_vector(768, 0.1f);
  auto vec2 = make_dummy_vector(768, 0.2f);
  TEST_ASSERT(db.insert_chunk_embedding((*chunk_ids)[0], vec1), "Insert chunk 1 embedding");
  TEST_ASSERT(db.insert_chunk_embedding((*chunk_ids)[1], vec2), "Insert chunk 2 embedding");

  // File record retrieval
  auto file_rec = db.get_file(file_path);
  TEST_ASSERT(file_rec.has_value(), "File record retrieved");
  TEST_ASSERT(file_rec->size == 1024, "File size matches");
  TEST_ASSERT(file_rec->mtime == 1700000000, "File mtime matches");

  // Search lexical match
  auto search_res = db.search_hybrid({}, "Revenue", 5);
  TEST_ASSERT(!search_res.empty(), "Lexical search found result");
  TEST_ASSERT(search_res[0].path == file_path, "Lexical search matched correct file");
  TEST_ASSERT(search_res[0].score > 0.0f, "Lexical search has positive score");

  // Search filename boost
  auto fn_res = db.search_hybrid({}, "financial", 5);
  TEST_ASSERT(!fn_res.empty(), "Filename match found result");
  TEST_ASSERT(fn_res[0].path == file_path, "Filename match matched correct file");

  // Remove file
  TEST_ASSERT(db.remove_file(file_path), "File removed");
  TEST_ASSERT(db.total_files() == 0, "Files count is 0 after removal");
  TEST_ASSERT(db.total_chunks() == 0, "Chunks count is 0 after removal");

  db.close();
  std::filesystem::remove(db_path);
  std::cout << "  ✓ Database CRUD passed." << std::endl;
}

void test_directory_removal_and_dedup()
{
  std::cout << "[Test] Directory removal and result deduplication..." << std::endl;

  const auto db_path = temp_db_path();
  lazyfinder::Database db;
  TEST_ASSERT(db.open(db_path.string()), "Database opened");

  // Insert 3 files in two directories
  db.insert_file_chunks(
      "/projects/app1/main.rs", 100, 1000, {"main.rs\nfn main() { run_server(); }"});
  db.insert_file_chunks("/projects/app1/config.toml", 50, 1000, {"config.toml\nport = 8080"});
  db.insert_file_chunks(
      "/projects/app2/main.rs", 120, 1000, {"main.rs\nfn main() { run_client(); }"});

  TEST_ASSERT(db.total_files() == 3, "Total files 3");

  // Multi-chunk file to test dedup
  db.insert_file_chunks("/docs/guide.md",
                        500,
                        1000,
                        {"guide.md\nServer installation section 1",
                         "guide.md\nServer configuration section 2",
                         "guide.md\nServer performance tuning section 3"});

  TEST_ASSERT(db.total_files() == 4, "Total files 4");

  // Search for "Server": /docs/guide.md has 3 matching chunks, but must only appear ONCE in results
  auto results = db.search_hybrid({}, "Server", 10);
  int guide_count = 0;
  for (const auto &res : results) {
    if (res.path == "/docs/guide.md") {
      guide_count++;
    }
  }
  TEST_ASSERT(guide_count == 1, "Multi-chunk file is deduplicated in results list");

  // Remove directory /projects/app1
  TEST_ASSERT(db.remove_directory("/projects/app1"), "Remove directory /projects/app1");
  TEST_ASSERT(db.total_files() == 2, "2 files remain (/projects/app2/main.rs and /docs/guide.md)");
  TEST_ASSERT(!db.get_file("/projects/app1/main.rs").has_value(), "app1/main.rs deleted");
  TEST_ASSERT(!db.get_file("/projects/app1/config.toml").has_value(), "app1/config.toml deleted");
  TEST_ASSERT(db.get_file("/projects/app2/main.rs").has_value(), "app2/main.rs preserved");

  db.close();
  std::filesystem::remove(db_path);
  std::cout << "  ✓ Directory removal and deduplication passed." << std::endl;
}

void test_query_intelligence_and_prefix()
{
  std::cout << "[Test] Query intelligence, camelCase and prefix search..." << std::endl;

  const auto db_path = temp_db_path();
  lazyfinder::Database db;
  TEST_ASSERT(db.open(db_path.string()), "Database opened");

  // Insert files with code identifiers and camelCase
  db.insert_file_chunks("/src/user_controller.cc",
                        200,
                        1000,
                        {"user_controller.cc\nvoid getUserProfileById(int userId) { return; }"});
  db.insert_file_chunks(
      "/src/auth.cc",
      150,
      1000,
      {"auth.cc\nbool validateSessionToken(const Token &token) { return true; }"});
  db.insert_file_chunks(
      "/docs/changelog.txt",
      300,
      1000,
      {"changelog.txt\nFixed database connection timeout issue in background worker thread."});

  // 1. Prefix query: "valid" expands to valid* matching validateSessionToken
  auto res_prefix = db.search_hybrid({}, "valid", 5);
  TEST_ASSERT(!res_prefix.empty(), "Prefix search for 'valid' matches validateSessionToken");
  TEST_ASSERT(res_prefix[0].path == "/src/auth.cc", "Matched auth.cc");

  // 2. camelCase query: "getUserProfile" splits into tokens matching getUserProfileById
  auto res_camel = db.search_hybrid({}, "getUserProfile", 5);
  TEST_ASSERT(!res_camel.empty(), "camelCase search finds getUserProfileById");
  TEST_ASSERT(res_camel[0].path == "/src/user_controller.cc", "Matched user_controller.cc");

  // 3. Quoted phrase search: "\"connection timeout\""
  auto res_phrase = db.search_hybrid({}, "\"connection timeout\"", 5);
  TEST_ASSERT(!res_phrase.empty(), "Quoted phrase matches changelog.txt");
  TEST_ASSERT(res_phrase[0].path == "/docs/changelog.txt", "Matched changelog.txt");

  db.close();
  std::filesystem::remove(db_path);
  std::cout << "  ✓ Query intelligence and prefix search passed." << std::endl;
}

void test_contextual_snippets_and_pages()
{
  std::cout << "[Test] Contextual windowed snippets and page/slide numbers..." << std::endl;

  const auto db_path = temp_db_path();
  lazyfinder::Database db;
  TEST_ASSERT(db.open(db_path.string()), "Database opened");

  // Multi-page PDF: 3 pages (chunk 0 = page 1, chunk 1 = page 2, chunk 2 = page 3)
  const std::string pdf_path = "/docs/manual.pdf";
  const std::vector<std::string> pdf_chunks = {
      "manual.pdf\nIntroduction to the platform and system prerequisites.",
      "manual.pdf\nSection 2: Installation instructions with PostgreSQL backend database setup and "
      "configuration.",
      "manual.pdf\nSection 3: Conclusion and troubleshooting common errors."};
  db.insert_file_chunks(pdf_path, 1000, 1000, pdf_chunks);

  // Multi-slide PPTX: 2 slides (chunk 0 = slide 1, chunk 1 = slide 2)
  const std::string pptx_path = "/slides/strategy.pptx";
  const std::vector<std::string> pptx_chunks = {
      "strategy.pptx\nSlide 1 Title: Q3 Engineering Goals and Deliverables",
      "strategy.pptx\nSlide 2: High Performance Vector Search and Hardware Acceleration"};
  db.insert_file_chunks(pptx_path, 2000, 1000, pptx_chunks);

  // Search for "PostgreSQL" in the PDF -> should match chunk 1 -> page 2
  auto res_pdf = db.search_hybrid({}, "PostgreSQL", 5);
  TEST_ASSERT(!res_pdf.empty(), "Found PDF result for PostgreSQL");
  TEST_ASSERT(res_pdf[0].path == pdf_path, "PDF path matches");
  TEST_ASSERT(res_pdf[0].page_number == 2, "Page number is 2 (chunk index 1 + 1)");
  TEST_ASSERT(res_pdf[0].snippet.find("Page 2 • ") != std::string::npos,
              "Snippet contains Page 2 badge");
  TEST_ASSERT(res_pdf[0].snippet.find("PostgreSQL") != std::string::npos,
              "Snippet contains PostgreSQL term");

  // Search for "Hardware" in PPTX -> should match chunk 1 -> slide 2
  auto res_pptx = db.search_hybrid({}, "Hardware", 5);
  TEST_ASSERT(!res_pptx.empty(), "Found PPTX result for Hardware");
  TEST_ASSERT(res_pptx[0].path == pptx_path, "PPTX path matches");
  TEST_ASSERT(res_pptx[0].page_number == 2, "Slide number is 2 (chunk index 1 + 1)");
  TEST_ASSERT(res_pptx[0].snippet.find("Slide 2 • ") != std::string::npos,
              "Snippet contains Slide 2 badge");

  // Media file with timestamps: lecture.mp4
  const std::string mp4_path = "/media/lecture.mp4";
  const std::vector<std::string> mp4_chunks = {
      "lecture.mp4\n[00:30] Course overview and syllabus introduction.",
      "lecture.mp4\n[02:15] Discussion on Kubernetes orchestration and cluster scalability.",
      "lecture.mp4\n[05:40] Final wrap up and next steps."};
  db.insert_file_chunks(mp4_path, 5000, 1000, mp4_chunks);

  // Search for "Kubernetes" in MP4 -> should match chunk 1 with timestamp [02:15] = 135 seconds
  auto res_media = db.search_hybrid({}, "Kubernetes", 5);
  TEST_ASSERT(!res_media.empty(), "Found media result for Kubernetes");
  TEST_ASSERT(res_media[0].path == mp4_path, "Media path matches");
  TEST_ASSERT(res_media[0].page_number == 135, "Media timestamp is 135 seconds (02:15)");
  TEST_ASSERT(res_media[0].snippet.find("02:15 • ") != std::string::npos,
              "Snippet contains 02:15 timestamp badge");

  db.close();
  std::filesystem::remove(db_path);
  std::cout << "  ✓ Contextual snippets, page numbers, and media timestamps passed." << std::endl;
}

void test_utf8_snippet_cleanliness()
{
  std::cout << "[Test] UTF-8 character preservation & mojibake prevention..." << std::endl;

  const auto db_path = temp_db_path();
  lazyfinder::Database db;
  TEST_ASSERT(db.open(db_path.string()), "Database opened");

  const std::string doc_path = "/docs/CV_Mayank.pdf";
  const std::vector<std::string> doc_chunks = {
      "CV_Mayank.pdf\nRust, Egui, Windows WASAPI, Discord API Leerio | AI PDF Intelligence "
      "Platform Link | Code • C++20, Qt6, Vulkan Acceleration"};
  db.insert_file_chunks(doc_path, 1024, 1000, doc_chunks);

  auto res = db.search_hybrid({}, "Vulkan", 5);
  TEST_ASSERT(!res.empty(), "Found result for Vulkan");
  TEST_ASSERT(res[0].page_number == 1, "Page number is 1");
  // Check badge contains clean bullet • (U+2022)
  TEST_ASSERT(res[0].snippet.find("Page 1 • ") != std::string::npos,
              "Snippet contains clean bullet badge Page 1 • ");
  // Check content contains clean bullet • (U+2022) and NO mojibake \xc3\xa2 (â)
  TEST_ASSERT(res[0].snippet.find("Code • C++20") != std::string::npos,
              "Snippet preserves internal bullet point without alteration");
  TEST_ASSERT(res[0].snippet.find("â") == std::string::npos,
              "Snippet has zero mojibake 'â' characters");

  db.close();
  std::filesystem::remove(db_path);
  std::cout << "  ✓ UTF-8 snippet cleanliness and mojibake prevention passed." << std::endl;
}

void test_video_chunks_and_thumbnails()
{
  std::cout << "[Test] Video chunks, timecodes, and thumbnail metadata..." << std::endl;

  const auto db_path = temp_db_path();
  lazyfinder::Database db;
  TEST_ASSERT(db.open(db_path.string()), "Database open");

  const std::string video_path = "/videos/screencast_demo.mp4";
  const std::vector<lazyfinder::ChunkRecord> chunks = {
      {"screencast_demo.mp4\n[00:15] Introduction to Vulkan compute pipelines",
       15,
       "/thumbs/video_15.jpg"},
      {"screencast_demo.mp4\n[02:45] Benchmarks and memory allocation performance",
       165,
       "/thumbs/video_165.jpg"},
      {"screencast_demo.mp4\n[05:10] Conclusion and next milestones",
       310,
       "/thumbs/video_310.jpg"}};

  auto chunk_ids = db.insert_file_chunks(video_path, 2048, 1700000000, chunks);
  TEST_ASSERT(chunk_ids.has_value() && chunk_ids->size() == 3,
              "Inserted 3 video chunks with metadata");

  // Search for "benchmarks"
  auto res = db.search_hybrid({}, "benchmarks", 5);
  TEST_ASSERT(!res.empty(), "Found video result for 'benchmarks'");
  TEST_ASSERT(res[0].path == video_path, "Matches correct video path");
  TEST_ASSERT(res[0].page_number == 165, "Timecode retrieved as 165 seconds (02:45)");
  TEST_ASSERT(res[0].thumbnail_path == "/thumbs/video_165.jpg", "Thumbnail path preserved");
  TEST_ASSERT(res[0].snippet.find("02:45 • ") != std::string::npos,
              "Formatted timecode badge in snippet");

  db.close();
  std::filesystem::remove(db_path);
  std::cout << "  ✓ Video chunks, timecodes, and thumbnail persistence passed." << std::endl;
}

void test_query_category_prefixes()
{
  std::cout << "[Test] Query Category Prefixes and Category Filtering..." << std::endl;

  // 1. Prefix parsing verification
  auto q1 = lazyfinder::Database::parse_query_category("code: calculate_hash");
  TEST_ASSERT(q1.category == lazyfinder::QueryCategory::Code, "Parsed 'code:' as Code");
  TEST_ASSERT(q1.text == "calculate_hash", "Extracted clean text 'calculate_hash'");

  auto q2 = lazyfinder::Database::parse_query_category("type:code fn main");
  TEST_ASSERT(q2.category == lazyfinder::QueryCategory::Code, "Parsed 'type:code' as Code");
  TEST_ASSERT(q2.text == "fn main", "Extracted clean text 'fn main'");

  auto q3 = lazyfinder::Database::parse_query_category("doc: annual report");
  TEST_ASSERT(q3.category == lazyfinder::QueryCategory::Document, "Parsed 'doc:' as Document");
  TEST_ASSERT(q3.text == "annual report", "Extracted clean text 'annual report'");

  auto q4 = lazyfinder::Database::parse_query_category("pdf: quarterly results");
  TEST_ASSERT(q4.category == lazyfinder::QueryCategory::Document, "Parsed 'pdf:' as Document");
  TEST_ASSERT(q4.text == "quarterly results", "Extracted clean text 'quarterly results'");

  auto q5 = lazyfinder::Database::parse_query_category("image: screenshot");
  TEST_ASSERT(q5.category == lazyfinder::QueryCategory::Image, "Parsed 'image:' as Image");
  TEST_ASSERT(q5.text == "screenshot", "Extracted clean text 'screenshot'");

  auto q6 = lazyfinder::Database::parse_query_category("video: conference");
  TEST_ASSERT(q6.category == lazyfinder::QueryCategory::Video, "Parsed 'video:' as Video");
  TEST_ASSERT(q6.text == "conference", "Extracted clean text 'conference'");

  auto q7 = lazyfinder::Database::parse_query_category("audio: podcast");
  TEST_ASSERT(q7.category == lazyfinder::QueryCategory::Audio, "Parsed 'audio:' as Audio");
  TEST_ASSERT(q7.text == "podcast", "Extracted clean text 'podcast'");

  auto q8 = lazyfinder::Database::parse_query_category("regular search text");
  TEST_ASSERT(q8.category == lazyfinder::QueryCategory::All, "Regular text parsed as All");
  TEST_ASSERT(q8.text == "regular search text", "Clean text preserved");

  // 2. Path matching verification
  TEST_ASSERT(
      lazyfinder::Database::path_matches_category("main.rs", lazyfinder::QueryCategory::Code),
      "main.rs is Code");
  TEST_ASSERT(
      !lazyfinder::Database::path_matches_category("main.rs", lazyfinder::QueryCategory::Image),
      "main.rs is NOT Image");
  TEST_ASSERT(
      lazyfinder::Database::path_matches_category("doc.pdf", lazyfinder::QueryCategory::Document),
      "doc.pdf is Document");
  TEST_ASSERT(
      lazyfinder::Database::path_matches_category("pic.png", lazyfinder::QueryCategory::Image),
      "pic.png is Image");
  TEST_ASSERT(
      lazyfinder::Database::path_matches_category("vid.mp4", lazyfinder::QueryCategory::Video),
      "vid.mp4 is Video");
  TEST_ASSERT(
      lazyfinder::Database::path_matches_category("song.mp3", lazyfinder::QueryCategory::Audio),
      "song.mp3 is Audio");
  TEST_ASSERT(
      lazyfinder::Database::path_matches_category("song.mp3", lazyfinder::QueryCategory::Media),
      "song.mp3 is Media");

  // 3. Category filtered hybrid search verification
  const std::string db_path = "/tmp/test_cat_prefix_" + std::to_string(std::rand()) + ".db";
  lazyfinder::Database db;
  TEST_ASSERT(db.open(db_path), "Database opened");

  db.insert_file_chunks("src/math.cpp", 100, 1000, {"int compute_value() { return 42; }"});
  db.insert_file_chunks("docs/report.pdf", 200, 1000, {"Annual compute_value financial summary"});
  db.insert_file_chunks("images/diagram.png", 300, 1000, {"Architecture diagram compute_value"});
  db.insert_file_chunks("videos/demo.mp4", 400, 1000, {"Demo clip compute_value presentation"});
  db.insert_file_chunks(
      "podcasts/show.mp3", 500, 1000, {"Podcast episode compute_value discussion"});

  // Query "code: compute_value" -> should return ONLY src/math.cpp
  auto code_res = db.search_hybrid({}, "code: compute_value", 10);
  TEST_ASSERT(code_res.size() == 1, "Only 1 result for 'code: compute_value'");
  TEST_ASSERT(code_res[0].path == "src/math.cpp", "Result is math.cpp");

  // Query "doc: compute_value" -> should return ONLY docs/report.pdf
  auto doc_res = db.search_hybrid({}, "doc: compute_value", 10);
  TEST_ASSERT(doc_res.size() == 1, "Only 1 result for 'doc: compute_value'");
  TEST_ASSERT(doc_res[0].path == "docs/report.pdf", "Result is report.pdf");

  // Query "image: compute_value" -> should return ONLY images/diagram.png
  auto img_res = db.search_hybrid({}, "image: compute_value", 10);
  TEST_ASSERT(img_res.size() == 1, "Only 1 result for 'image: compute_value'");
  TEST_ASSERT(img_res[0].path == "images/diagram.png", "Result is diagram.png");

  // Query "video: compute_value" -> should return ONLY videos/demo.mp4
  auto vid_res = db.search_hybrid({}, "video: compute_value", 10);
  TEST_ASSERT(vid_res.size() == 1, "Only 1 result for 'video: compute_value'");
  TEST_ASSERT(vid_res[0].path == "videos/demo.mp4", "Result is demo.mp4");

  // Query "audio: compute_value" -> should return ONLY podcasts/show.mp3
  auto aud_res = db.search_hybrid({}, "audio: compute_value", 10);
  TEST_ASSERT(aud_res.size() == 1, "Only 1 result for 'audio: compute_value'");
  TEST_ASSERT(aud_res[0].path == "podcasts/show.mp3", "Result is show.mp3");

  // Query without prefix -> returns all matching files
  auto all_res = db.search_hybrid({}, "compute_value", 10);
  TEST_ASSERT(all_res.size() == 5, "Unfiltered query returns all 5 matching files");

  db.close();
  std::filesystem::remove(db_path);
  std::cout << "  ✓ Query category prefixes and filtering passed." << std::endl;
}

int main()
{
  std::cout << "========================================" << std::endl;
  std::cout << "LazyFinder Database & Ingestion Tests   " << std::endl;
  std::cout << "========================================" << std::endl;

  test_database_crud();
  test_directory_removal_and_dedup();
  test_query_intelligence_and_prefix();
  test_contextual_snippets_and_pages();
  test_utf8_snippet_cleanliness();
  test_video_chunks_and_thumbnails();
  test_query_category_prefixes();

  std::cout << "========================================" << std::endl;
  std::cout << "ALL DATABASE TESTS PASSED SUCCESSFULLY! " << std::endl;
  std::cout << "========================================" << std::endl;

  return 0;
}
