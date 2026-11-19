#include "database.hh"
#include "embedder.hh"
#include "indexer.hh"
#include "watcher.hh"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
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

std::filesystem::path create_temp_dir()
{
  auto dir = std::filesystem::temp_directory_path() /
             ("lazyfinder_test_idx_" + std::to_string(std::rand()));
  std::filesystem::create_directories(dir);
  return dir;
}

void write_test_file(const std::filesystem::path &path, std::string_view content)
{
  std::ofstream out(path, std::ios::binary);
  out << content;
}

void test_indexer_ignore_patterns()
{
  std::cout << "[Test] Indexer Ignore Patterns & Filtering..." << std::endl;

  lazyfinder::Database db;
  lazyfinder::Embedder embedder;
  lazyfinder::Indexer indexer(db, embedder);

  // Hidden folders and files
  TEST_ASSERT(indexer.is_ignored("/home/user/.git/config"), "Git directory must be ignored");
  TEST_ASSERT(indexer.is_ignored("/home/user/.cache/data"), "Cache directory must be ignored");
  TEST_ASSERT(indexer.is_ignored("/home/user/project/.env"), "Hidden dotfile must be ignored");
  TEST_ASSERT(indexer.is_ignored("/home/user/node_modules/package.json"), "node_modules must be ignored");
  TEST_ASSERT(indexer.is_ignored("/home/user/project/build/output.o"), "build directory must be ignored");

  // Normal files must not be ignored
  TEST_ASSERT(!indexer.is_ignored("/home/user/Documents/report.pdf"), "Normal PDF must not be ignored");
  TEST_ASSERT(!indexer.is_ignored("/home/user/src/main.cc"), "Source code must not be ignored");
  TEST_ASSERT(!indexer.is_ignored("/home/user/notes.md"), "Markdown file must not be ignored");

  // Custom ignore patterns
  indexer.add_ignore_pattern("*.backup");
  TEST_ASSERT(indexer.is_ignored("/home/user/database.backup"), "Custom pattern *.backup ignored");

  std::cout << "  ✓ Ignore rules and pattern filtering passed." << std::endl;
}

void test_indexer_crud_lifecycle()
{
  std::cout << "[Test] Indexer File Ingestion and Removal..." << std::endl;

  auto temp_dir = create_temp_dir();
  auto db_path = temp_dir / "test_indexer.db";

  lazyfinder::Database db;
  TEST_ASSERT(db.open(db_path.string()), "Database open must succeed");

  lazyfinder::Embedder embedder;
  lazyfinder::Indexer indexer(db, embedder);

  // 1. Create a sample markdown file
  auto doc_path = temp_dir / "architecture.md";
  write_test_file(doc_path, "# System Architecture\nThis document describes high performance indexing and vector search.");

  // 2. Index the file
  bool indexed = indexer.index_file(doc_path);
  TEST_ASSERT(indexed, "index_file must succeed on valid document");
  TEST_ASSERT(db.total_files() == 1, "Database total files must be 1");
  TEST_ASSERT(db.total_chunks() >= 1, "Database chunks must be >= 1");

  // 3. Search via FTS keyword
  auto results = db.search_hybrid({}, "architecture indexing", 10);
  TEST_ASSERT(!results.empty(), "Search must find newly indexed document");
  TEST_ASSERT(results[0].path == doc_path.string(), "Result path matches indexed file");

  // 4. Remove the file
  bool removed = indexer.remove_file(doc_path);
  TEST_ASSERT(removed, "remove_file must succeed");
  TEST_ASSERT(db.total_files() == 0, "Database total files must be 0 after removal");
  TEST_ASSERT(db.total_chunks() == 0, "Database chunks must be 0 after removal");

  // 5. Search again - must return 0 results
  auto empty_results = db.search_hybrid({}, "architecture indexing", 10);
  TEST_ASSERT(empty_results.empty(), "Search must return no results after removal");

  db.close();
  std::error_code ec;
  std::filesystem::remove_all(temp_dir, ec);

  std::cout << "  ✓ Indexer ingestion and removal lifecycle passed." << std::endl;
}

void test_watcher_lifecycle()
{
  std::cout << "[Test] Inotify Watcher Lifecycle & Event Detection..." << std::endl;

  auto watch_dir = create_temp_dir();

  lazyfinder::Watcher watcher;
  TEST_ASSERT(!watcher.is_running(), "Watcher is initially stopped");

  std::mutex mtx;
  std::vector<std::string> detected_changes;

  bool started = watcher.start({watch_dir.string()}, [&](const std::vector<std::string> &changed_paths) {
    std::lock_guard<std::mutex> lock(mtx);
    for (const auto &p : changed_paths) {
      detected_changes.push_back(p);
    }
  });

  TEST_ASSERT(started, "Watcher start must succeed on valid directory");
  TEST_ASSERT(watcher.is_running(), "Watcher must report running state");

  // Create a file in the watched directory
  auto test_file = watch_dir / "live_change.txt";
  write_test_file(test_file, "First content");

  // Allow inotify event loop brief time to process (Watcher has a 3s debounce)
  for (int i = 0; i < 75; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::lock_guard<std::mutex> lock(mtx);
    if (!detected_changes.empty()) {
      break;
    }
  }

  {
    std::lock_guard<std::mutex> lock(mtx);
    TEST_ASSERT(!detected_changes.empty(), "Watcher must detect file creation/modification");
  }

  // Stop watcher
  watcher.stop();
  TEST_ASSERT(!watcher.is_running(), "Watcher reports stopped state after stop()");

  std::error_code ec;
  std::filesystem::remove_all(watch_dir, ec);

  std::cout << "  ✓ Inotify watcher event detection passed." << std::endl;
}

} // namespace

int main()
{
  std::cout << "========================================" << std::endl;
  std::cout << "LazyFinder Indexer & Watcher Tests      " << std::endl;
  std::cout << "========================================" << std::endl;

  test_indexer_ignore_patterns();
  test_indexer_crud_lifecycle();
  test_watcher_lifecycle();

  std::cout << "========================================" << std::endl;
  std::cout << "ALL INDEXER & WATCHER TESTS PASSED!     " << std::endl;
  std::cout << "========================================" << std::endl;

  return 0;
}
