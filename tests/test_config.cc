#include "config.hh"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

} // namespace

void test_path_expansion()
{
  std::cout << "[Test] Path expansion and collapse..." << std::endl;

  const char *home = std::getenv("HOME");
  if (home && *home) {
    const std::string home_str(home);
    TEST_ASSERT(lazyfinder::Config::expand_path("~") == home_str, "Expand ~ matches HOME");
    TEST_ASSERT(lazyfinder::Config::expand_path("~/Documents") == home_str + "/Documents",
                "Expand ~/Documents matches HOME/Documents");

    TEST_ASSERT(lazyfinder::Config::collapse_path(home_str) == "~", "Collapse HOME to ~");
    TEST_ASSERT(lazyfinder::Config::collapse_path(home_str + "/Videos") == "~/Videos",
                "Collapse HOME/Videos to ~/Videos");
  }
}

void test_model_search_directories()
{
  std::cout << "[Test] Model search directories..." << std::endl;

  const auto dirs = lazyfinder::Config::model_search_directories();
  TEST_ASSERT(!dirs.empty(), "Search directories should not be empty");

  bool has_system_dir = false;
  bool has_user_dir = false;

  for (const auto &d : dirs) {
    const std::string s = d.string();
    if (s.find("/usr/share/lazyfinder/models") != std::string::npos) {
      has_system_dir = true;
    }
    if (s.find("lazyfinder/models") != std::string::npos) {
      has_user_dir = true;
    }
  }

  TEST_ASSERT(has_system_dir, "Should include system directory /usr/share/lazyfinder/models");
  TEST_ASSERT(has_user_dir, "Should include user data directory");
}

void test_model_resolution()
{
  std::cout << "[Test] Model resolution and installed check..." << std::endl;

  const std::string default_path = lazyfinder::Config::default_user_model_path();
  TEST_ASSERT(!default_path.empty(), "Default user model path is non-empty");
  TEST_ASSERT(default_path.ends_with("embeddinggemma-2-Q8_0.gguf"),
              "Default model filename is embeddinggemma-2-Q8_0.gguf");

  // Non-existent model check
  TEST_ASSERT(!lazyfinder::Config::is_model_installed("non_existent_model_12345.gguf"),
              "Non-existent model must return false");

  // Resolve model should return a non-empty string path
  const std::string resolved = lazyfinder::Config::resolve_model_path();
  TEST_ASSERT(!resolved.empty(), "Resolved model path must not be empty");
}

void test_environment_override()
{
  std::cout << "[Test] Environment variable LAZYFINDER_MODELS_DIR override..." << std::endl;

  // Create temporary directory with a dummy model
  const auto temp_dir = std::filesystem::temp_directory_path() / "lazyfinder_test_models";
  std::error_code ec;
  std::filesystem::create_directories(temp_dir, ec);

  const auto dummy_model = temp_dir / "embeddinggemma-2-Q8_0.gguf";
  {
    std::ofstream ofs(dummy_model);
    ofs << "dummy model bytes for test" << std::endl;
  }

  // Set env var
  setenv("LAZYFINDER_MODELS_DIR", temp_dir.string().c_str(), 1);

  const auto dirs = lazyfinder::Config::model_search_directories();
  TEST_ASSERT(!dirs.empty(), "Directories not empty");
  TEST_ASSERT(dirs[0] == temp_dir, "First directory must be the env override");

  const std::string resolved = lazyfinder::Config::resolve_model_path();
  TEST_ASSERT(resolved == dummy_model.string(), "Resolved model matches env override directory");
  TEST_ASSERT(lazyfinder::Config::is_model_installed(), "Model is detected as installed");

  // Clean up
  unsetenv("LAZYFINDER_MODELS_DIR");
  std::filesystem::remove_all(temp_dir, ec);
}

int main()
{
  std::cout << "Running LazyFinder Config & Model Discovery Unit Tests..." << std::endl;
  test_path_expansion();
  test_model_search_directories();
  test_model_resolution();
  test_environment_override();

  std::cout << "All Config & Model Discovery tests passed successfully!" << std::endl;
  return 0;
}
