#include "extractor.hh"

#include <algorithm>
#include <fstream>
#include <unordered_set>

#ifdef HAVE_POPPLER
#include <poppler-document.h>
#include <poppler-page.h>
#endif

namespace lazyfinder
{

namespace
{

constexpr size_t max_file_size = 10 * 1024 * 1024;
constexpr size_t max_chunk_chars = 1500;
constexpr size_t chunk_overlap_chars = 200;

const std::unordered_set<std::string> &supported_extensions()
{
  static const std::unordered_set<std::string> exts = {
      ".txt", ".md",   ".markdown", ".rst",  ".org", ".csv",  ".log", ".c",    ".h",
      ".cc",  ".cpp",  ".cxx",      ".hh",   ".hpp", ".rs",   ".py",  ".js",   ".ts",
      ".jsx", ".tsx",  ".go",       ".java", ".sh",  ".bash", ".zsh", ".json", ".yaml",
      ".yml", ".toml", ".xml",      ".html", ".css", ".sql",  ".pdf"};
  return exts;
}

bool is_binary_file(const std::filesystem::path &path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return true;
  }

  char buffer[1024];
  file.read(buffer, sizeof(buffer));
  const std::streamsize bytes_read = file.gcount();

  for (std::streamsize i = 0; i < bytes_read; ++i) {
    if (buffer[i] == '\0') {
      return true;
    }
  }
  return false;
}

std::vector<std::string> chunk_text(std::string_view text)
{
  if (text.empty()) {
    return {};
  }

  if (text.size() <= max_chunk_chars) {
    return {std::string(text)};
  }

  std::vector<std::string> chunks;
  size_t start = 0;
  const size_t step = max_chunk_chars - chunk_overlap_chars;

  while (start < text.size()) {
    const size_t len = std::min(max_chunk_chars, text.size() - start);
    chunks.emplace_back(text.substr(start, len));
    start += step;
  }

  return chunks;
}

} // namespace

bool Extractor::is_supported(const std::filesystem::path &path)
{
  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

  return supported_extensions().contains(ext);
}

std::vector<std::string> Extractor::extract_chunks(const std::filesystem::path &path)
{
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || size == 0 || size > max_file_size) {
    return {};
  }

  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

#ifdef HAVE_POPPLER
  if (ext == ".pdf") {
    return extract_pdf_file(path);
  }
#endif

  if (is_binary_file(path)) {
    return {};
  }

  return extract_text_file(path);
}

std::vector<std::string> Extractor::extract_text_file(const std::filesystem::path &path)
{
  std::ifstream file(path);
  if (!file) {
    return {};
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  return chunk_text(content);
}

#ifdef HAVE_POPPLER
std::vector<std::string> Extractor::extract_pdf_file(const std::filesystem::path &path)
{
  std::unique_ptr<poppler::document> doc(poppler::document::load_from_file(path.string()));
  if (!doc || doc->is_locked()) {
    return {};
  }

  std::vector<std::string> chunks;
  const int num_pages = doc->pages();

  for (int i = 0; i < num_pages; ++i) {
    std::unique_ptr<poppler::page> page(doc->create_page(i));
    if (!page) {
      continue;
    }

    std::string page_text = page->text().to_utf8();
    if (!page_text.empty()) {
      auto page_chunks = chunk_text(page_text);
      chunks.insert(chunks.end(),
                    std::make_move_iterator(page_chunks.begin()),
                    std::make_move_iterator(page_chunks.end()));
    }
  }

  return chunks;
}
#endif

} // namespace lazyfinder
