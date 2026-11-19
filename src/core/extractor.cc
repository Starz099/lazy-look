#include "extractor.hh"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <unordered_set>

#ifdef HAVE_POPPLER
#include <poppler-document.h>
#include <poppler-page.h>
#endif

namespace lazyfinder
{

namespace
{

constexpr size_t max_file_size = 20 * 1024 * 1024;
constexpr size_t max_chunk_chars = 2500;
constexpr size_t chunk_overlap_chars = 150;
constexpr size_t max_chunks_per_file = 25;

const std::unordered_set<std::string> &code_extensions()
{
  static const std::unordered_set<std::string> exts = {
      ".c",   ".h",    ".cc",  ".cpp",  ".cxx",  ".hh",  ".hpp",  ".rs",  ".py",   ".js",
      ".ts",  ".jsx",  ".tsx", ".go",   ".java", ".sh",  ".bash", ".zsh", ".json", ".yaml",
      ".yml", ".toml", ".xml", ".html", ".css",  ".sql", ".lua",  ".zig", ".cmake"};
  return exts;
}

const std::unordered_set<std::string> &supported_extensions()
{
  static const std::unordered_set<std::string> exts = {
      ".txt",  ".md",   ".markdown", ".rst",  ".org", ".csv",   ".log",  ".c",   ".h",    ".cc",
      ".cpp",  ".cxx",  ".hh",       ".hpp",  ".rs",  ".py",    ".js",   ".ts",  ".jsx",  ".tsx",
      ".go",   ".java", ".sh",       ".bash", ".zsh", ".json",  ".yaml", ".yml", ".toml", ".xml",
      ".html", ".css",  ".sql",      ".lua",  ".zig", ".cmake", ".pdf",  ".docx"};
  return exts;
}

std::string escape_shell_arg(const std::string &arg)
{
  std::string escaped = "'";
  for (const char c : arg) {
    if (c == '\'') {
      escaped += "'\\''";
    }
    else {
      escaped += c;
    }
  }
  escaped += "'";
  return escaped;
}

std::string clean_text(std::string_view text)
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

std::string clean_xml_text(std::string_view xml)
{
  std::string result;
  result.reserve(xml.size());
  bool inside_tag = false;
  bool last_space = false;

  for (size_t i = 0; i < xml.size(); ++i) {
    const char c = xml[i];
    if (c == '<') {
      inside_tag = true;
      if (!last_space) {
        result.push_back(' ');
        last_space = true;
      }
    }
    else if (c == '>') {
      inside_tag = false;
    }
    else if (!inside_tag) {
      if (c == '&') {
        if (xml.substr(i, 5) == "&amp;") {
          result.push_back('&');
          i += 4;
        }
        else if (xml.substr(i, 4) == "&lt;") {
          result.push_back('<');
          i += 3;
        }
        else if (xml.substr(i, 4) == "&gt;") {
          result.push_back('>');
          i += 3;
        }
        else if (xml.substr(i, 6) == "&quot;") {
          result.push_back('"');
          i += 5;
        }
        else if (xml.substr(i, 6) == "&apos;") {
          result.push_back('\'');
          i += 5;
        }
        else {
          result.push_back(c);
        }
        last_space = false;
      }
      else if (c == '\r' || c == '\n' || c == '\t') {
        if (!last_space) {
          result.push_back(' ');
          last_space = true;
        }
      }
      else if (static_cast<unsigned char>(c) >= 32) {
        result.push_back(c);
        last_space = (c == ' ');
      }
    }
  }
  return result;
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

std::vector<std::string> chunk_text(std::string_view text, size_t max_chunks = max_chunks_per_file)
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

  while (start < text.size() && chunks.size() < max_chunks) {
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

  std::vector<std::string> chunks;
  if (ext == ".pdf") {
    chunks = extract_pdf_file(path);
  }
  else if (ext == ".docx") {
    chunks = extract_docx_file(path);
  }
  else if (is_binary_file(path)) {
    return {path.filename().string()};
  }
  else {
    chunks = extract_text_file(path);
  }

  // Fallback: If document has 0 extractable text (e.g. image-only PDF scan),
  // index the filename so the file is discoverable and never causes sync error.
  if (chunks.empty()) {
    chunks.push_back(path.filename().string());
  }

  // Prepend filename context so FTS5 lexical matching and semantic vector
  // embeddings index both filename keywords and body text together.
  const std::string fn = path.filename().string();
  for (auto &chunk : chunks) {
    if (chunk != fn && !chunk.starts_with(fn)) {
      chunk = fn + "\n" + chunk;
    }
  }

  return chunks;
}

std::vector<std::string> Extractor::extract_text_file(const std::filesystem::path &path)
{
  std::ifstream file(path);
  if (!file) {
    return {};
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (content.empty()) {
    return {};
  }

  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

  if (code_extensions().contains(ext)) {
    if (content.size() <= 3500) {
      return {std::move(content)};
    }
    return chunk_text(content, 6);
  }

  return chunk_text(content, max_chunks_per_file);
}

std::vector<std::string> Extractor::extract_pdf_file(const std::filesystem::path &path)
{
#ifdef HAVE_POPPLER
  std::unique_ptr<poppler::document> doc(poppler::document::load_from_file(path.string()));
  if (doc && !doc->is_locked()) {
    std::vector<std::string> chunks;
    const int num_pages = doc->pages();
    const int pages_to_scan = std::min<int>(num_pages, static_cast<int>(max_chunks_per_file));

    for (int i = 0; i < pages_to_scan; ++i) {
      std::unique_ptr<poppler::page> page(doc->create_page(i));
      if (!page) {
        continue;
      }

      std::string page_text = clean_text(page->text().to_utf8());
      if (page_text.size() >= 15) {
        if (page_text.size() > max_chunk_chars) {
          page_text.resize(max_chunk_chars);
        }
        chunks.push_back(std::move(page_text));
        if (chunks.size() >= max_chunks_per_file) {
          break;
        }
      }
    }
    if (!chunks.empty()) {
      return chunks;
    }
  }
#endif

  // Robust fallback / standalone: pdftotext CLI
  const std::string cmd = "pdftotext -layout " + escape_shell_arg(path.string()) + " - 2>/dev/null";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return {};
  }

  std::vector<std::string> chunks;
  std::string current_page;
  char buffer[4096];

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    for (char *p = buffer; *p != '\0'; ++p) {
      if (*p == '\x0c') {
        std::string cleaned = clean_text(current_page);
        if (cleaned.size() >= 15) {
          if (cleaned.size() > max_chunk_chars) {
            cleaned.resize(max_chunk_chars);
          }
          chunks.push_back(std::move(cleaned));
          if (chunks.size() >= max_chunks_per_file) {
            pclose(pipe);
            return chunks;
          }
        }
        current_page.clear();
      }
      else {
        current_page.push_back(*p);
      }
    }
  }
  pclose(pipe);

  if (!current_page.empty()) {
    std::string cleaned = clean_text(current_page);
    if (cleaned.size() >= 15 && chunks.size() < max_chunks_per_file) {
      if (cleaned.size() > max_chunk_chars) {
        cleaned.resize(max_chunk_chars);
      }
      chunks.push_back(std::move(cleaned));
    }
  }

  return chunks;
}

std::vector<std::string> Extractor::extract_docx_file(const std::filesystem::path &path)
{
  const std::string cmd =
      "unzip -p " + escape_shell_arg(path.string()) + " word/document.xml 2>/dev/null";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return {};
  }

  std::string raw_xml;
  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    raw_xml += buffer;
  }
  pclose(pipe);

  if (raw_xml.empty()) {
    return {};
  }

  const std::string cleaned = clean_xml_text(raw_xml);
  return chunk_text(cleaned, max_chunks_per_file);
}

} // namespace lazyfinder
