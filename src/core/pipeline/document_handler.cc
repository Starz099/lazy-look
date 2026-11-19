#include "pipeline/document_handler.hh"

#include <algorithm>
#include <cctype>
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

const std::unordered_set<std::string> &document_extensions()
{
  static const std::unordered_set<std::string> exts = {// Plain text, markdown, documentation
                                                       ".txt",
                                                       ".md",
                                                       ".markdown",
                                                       ".rst",
                                                       ".org",
                                                       ".csv",
                                                       ".tsv",
                                                       ".log",
                                                       // Code files
                                                       ".c",
                                                       ".h",
                                                       ".cc",
                                                       ".cpp",
                                                       ".cxx",
                                                       ".hh",
                                                       ".hpp",
                                                       ".rs",
                                                       ".py",
                                                       ".js",
                                                       ".ts",
                                                       ".jsx",
                                                       ".tsx",
                                                       ".go",
                                                       ".java",
                                                       ".sh",
                                                       ".bash",
                                                       ".zsh",
                                                       ".json",
                                                       ".yaml",
                                                       ".yml",
                                                       ".toml",
                                                       ".xml",
                                                       ".html",
                                                       ".htm",
                                                       ".css",
                                                       ".sql",
                                                       ".lua",
                                                       ".zig",
                                                       ".cmake",
                                                       // Document and Office formats
                                                       ".pdf",
                                                       ".docx",
                                                       ".xlsx",
                                                       ".pptx",
                                                       ".odt",
                                                       ".ods",
                                                       ".odp",
                                                       ".epub",
                                                       ".rtf"};
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
  bool last_space = true;

  for (size_t i = 0; i < xml.size(); ++i) {
    const char c = xml[i];
    if (c == '<') {
      inside_tag = true;
      if (!last_space && !result.empty()) {
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
        else if (xml.substr(i, 6) == "&nbsp;") {
          if (!last_space && !result.empty()) {
            result.push_back(' ');
            last_space = true;
          }
          i += 5;
          continue;
        }
        else {
          result.push_back(c);
        }
        last_space = false;
      }
      else if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
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
  }
  while (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  return result;
}

std::string clean_html_text(std::string_view html)
{
  std::string result;
  result.reserve(html.size());
  bool in_tag = false;
  bool in_script = false;
  bool in_style = false;
  bool last_space = true;

  for (size_t i = 0; i < html.size(); ++i) {
    const char c = html[i];
    if (c == '<') {
      in_tag = true;
      if (i + 7 < html.size() &&
          (html.substr(i + 1, 6) == "script" || html.substr(i + 1, 6) == "SCRIPT")) {
        in_script = true;
      }
      else if (i + 8 < html.size() &&
               (html.substr(i + 1, 7) == "/script" || html.substr(i + 1, 7) == "/SCRIPT")) {
        in_script = false;
      }
      else if (i + 6 < html.size() &&
               (html.substr(i + 1, 5) == "style" || html.substr(i + 1, 5) == "STYLE")) {
        in_style = true;
      }
      else if (i + 7 < html.size() &&
               (html.substr(i + 1, 6) == "/style" || html.substr(i + 1, 6) == "/STYLE")) {
        in_style = false;
      }
      if (!last_space && !result.empty()) {
        result.push_back(' ');
        last_space = true;
      }
    }
    else if (c == '>') {
      in_tag = false;
    }
    else if (!in_tag && !in_script && !in_style) {
      if (c == '&') {
        if (html.substr(i, 6) == "&nbsp;") {
          if (!last_space && !result.empty()) {
            result.push_back(' ');
            last_space = true;
          }
          i += 5;
        }
        else if (html.substr(i, 5) == "&amp;") {
          result.push_back('&');
          last_space = false;
          i += 4;
        }
        else if (html.substr(i, 4) == "&lt;") {
          result.push_back('<');
          last_space = false;
          i += 3;
        }
        else if (html.substr(i, 4) == "&gt;") {
          result.push_back('>');
          last_space = false;
          i += 3;
        }
        else if (html.substr(i, 6) == "&quot;") {
          result.push_back('"');
          last_space = false;
          i += 5;
        }
        else if (html.substr(i, 6) == "&apos;" || html.substr(i, 5) == "&#39;") {
          result.push_back('\'');
          last_space = false;
          i += (html.substr(i, 6) == "&apos;" ? 5 : 4);
        }
        else {
          result.push_back(c);
          last_space = false;
        }
      }
      else if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
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
  }

  while (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  return result;
}

std::string extract_rtf_text(std::string_view rtf)
{
  std::string result;
  result.reserve(rtf.size());
  int brace_depth = 0;
  bool skip_group = false;
  int skip_depth = 0;

  for (size_t i = 0; i < rtf.size(); ++i) {
    char c = rtf[i];
    if (c == '{') {
      brace_depth++;
      if (i + 2 < rtf.size() && rtf[i + 1] == '\\' && rtf[i + 2] == '*') {
        skip_group = true;
        skip_depth = brace_depth;
      }
      else if (i + 8 < rtf.size() && rtf.substr(i + 1, 8) == "\\fonttbl") {
        skip_group = true;
        skip_depth = brace_depth;
      }
      else if (i + 9 < rtf.size() && rtf.substr(i + 1, 9) == "\\colortbl") {
        skip_group = true;
        skip_depth = brace_depth;
      }
      continue;
    }
    if (c == '}') {
      if (skip_group && brace_depth == skip_depth) {
        skip_group = false;
      }
      if (brace_depth > 0) {
        brace_depth--;
      }
      continue;
    }
    if (skip_group) {
      continue;
    }

    if (c == '\\') {
      i++;
      if (i >= rtf.size()) {
        break;
      }
      if (rtf[i] == '\\' || rtf[i] == '{' || rtf[i] == '}') {
        result.push_back(rtf[i]);
      }
      else if (rtf[i] == '\'') {
        if (i + 2 < rtf.size()) {
          char hex[3] = {rtf[i + 1], rtf[i + 2], 0};
          char *end = nullptr;
          long val = std::strtol(hex, &end, 16);
          if (end == hex + 2 && val > 0) {
            result.push_back(static_cast<char>(val));
          }
          i += 2;
        }
      }
      else {
        std::string control;
        while (i < rtf.size() && std::isalpha(static_cast<unsigned char>(rtf[i]))) {
          control += rtf[i];
          i++;
        }
        if (i < rtf.size() && (rtf[i] == '-' || std::isdigit(static_cast<unsigned char>(rtf[i])))) {
          while (i < rtf.size() &&
                 (rtf[i] == '-' || std::isdigit(static_cast<unsigned char>(rtf[i])))) {
            i++;
          }
        }
        if (i < rtf.size() && rtf[i] == ' ') {
          // delimiter space after control word
        }
        else {
          i--;
        }
        if (control == "par" || control == "line") {
          result.push_back('\n');
        }
        else if (control == "tab") {
          result.push_back(' ');
        }
      }
    }
    else if (c == '\r' || c == '\n') {
      // Ignored in RTF format
    }
    else {
      result.push_back(c);
    }
  }

  return clean_text(result);
}

std::string run_unzip(const std::filesystem::path &path, const std::string &pattern)
{
  const std::string cmd =
      "unzip -p " + escape_shell_arg(path.string()) + " " + pattern + " 2>/dev/null";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return {};
  }

  std::string output;
  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
    if (output.size() >= max_file_size) {
      break;
    }
  }
  pclose(pipe);
  return output;
}

std::vector<ProcessedChunk>
to_chunks(std::string_view text, int page_or_ts = 0, size_t max_chunks = max_chunks_per_file)
{
  if (text.empty()) {
    return {};
  }

  if (text.size() <= max_chunk_chars) {
    return {ProcessedChunk{std::string(text), page_or_ts, {}}};
  }

  std::vector<ProcessedChunk> chunks;
  size_t start = 0;
  const size_t step = max_chunk_chars - chunk_overlap_chars;

  while (start < text.size() && chunks.size() < max_chunks) {
    const size_t len = std::min(max_chunk_chars, text.size() - start);
    chunks.push_back(ProcessedChunk{std::string(text.substr(start, len)), page_or_ts, {}});
    start += step;
  }

  return chunks;
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

} // namespace

FileCategory DocumentHandler::category() const
{
  return FileCategory::Document;
}

std::string_view DocumentHandler::name() const
{
  return "DocumentHandler";
}

bool DocumentHandler::supports(const std::filesystem::path &path) const
{
  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

  return document_extensions().contains(ext);
}

ProcessedFile DocumentHandler::process(const std::filesystem::path &path)
{
  ProcessedFile result;
  result.path = path;
  result.category = FileCategory::Document;

  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || size == 0 || size > max_file_size) {
    return result;
  }

  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

  if (ext == ".pdf") {
    result.chunks = extract_pdf(path);
  }
  else if (ext == ".docx") {
    result.chunks = extract_docx(path);
  }
  else if (ext == ".xlsx") {
    result.chunks = extract_xlsx(path);
  }
  else if (ext == ".pptx") {
    result.chunks = extract_pptx(path);
  }
  else if (ext == ".odt" || ext == ".ods" || ext == ".odp") {
    result.chunks = extract_opendocument(path);
  }
  else if (ext == ".epub") {
    result.chunks = extract_epub(path);
  }
  else if (ext == ".rtf") {
    result.chunks = extract_rtf(path);
  }
  else if (ext == ".html" || ext == ".htm") {
    result.chunks = extract_html(path);
  }
  else if (is_binary_file(path)) {
    result.chunks = {ProcessedChunk{path.filename().string(), 0, {}}};
    return result;
  }
  else {
    result.chunks = extract_text(path);
  }

  if (result.chunks.empty()) {
    result.chunks.push_back(ProcessedChunk{path.filename().string(), 0, {}});
  }

  const std::string fn = path.filename().string();
  for (auto &chunk : result.chunks) {
    if (chunk.text != fn && !chunk.text.starts_with(fn)) {
      chunk.text = fn + "\n" + chunk.text;
    }
  }

  return result;
}

std::vector<ProcessedChunk> DocumentHandler::extract_text(const std::filesystem::path &path)
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
      return {ProcessedChunk{std::move(content), 0, {}}};
    }
    return to_chunks(content, 0, 6);
  }

  return to_chunks(content, 0, max_chunks_per_file);
}

std::vector<ProcessedChunk> DocumentHandler::extract_html(const std::filesystem::path &path)
{
  std::ifstream file(path);
  if (!file) {
    return {};
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (content.empty()) {
    return {};
  }

  const std::string cleaned = clean_html_text(content);
  return to_chunks(cleaned, 0, max_chunks_per_file);
}

std::vector<ProcessedChunk> DocumentHandler::extract_pdf(const std::filesystem::path &path)
{
#ifdef HAVE_POPPLER
  std::unique_ptr<poppler::document> doc(poppler::document::load_from_file(path.string()));
  if (doc && !doc->is_locked()) {
    std::vector<ProcessedChunk> chunks;
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
        chunks.push_back(ProcessedChunk{std::move(page_text), i + 1, {}});
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

  const std::string cmd = "pdftotext -layout " + escape_shell_arg(path.string()) + " - 2>/dev/null";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return {};
  }

  std::vector<ProcessedChunk> chunks;
  std::string current_page;
  char buffer[4096];
  int page_num = 1;

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    for (char *p = buffer; *p != '\0'; ++p) {
      if (*p == '\x0c') {
        std::string cleaned = clean_text(current_page);
        if (cleaned.size() >= 15) {
          if (cleaned.size() > max_chunk_chars) {
            cleaned.resize(max_chunk_chars);
          }
          chunks.push_back(ProcessedChunk{std::move(cleaned), page_num, {}});
          if (chunks.size() >= max_chunks_per_file) {
            pclose(pipe);
            return chunks;
          }
        }
        current_page.clear();
        page_num++;
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
      chunks.push_back(ProcessedChunk{std::move(cleaned), page_num, {}});
    }
  }

  // If no embedded text was found, document is a scanned PDF: try Tesseract OCR fallback
  if (chunks.empty()) {
    static const bool has_tesseract = (std::system("which tesseract >/dev/null 2>&1") == 0);
    if (has_tesseract) {
      const std::string ocr_cmd =
          "tesseract " + escape_shell_arg(path.string()) + " stdout 2>/dev/null";
      FILE *ocr_pipe = popen(ocr_cmd.c_str(), "r");
      if (ocr_pipe) {
        current_page.clear();
        page_num = 1;
        while (fgets(buffer, sizeof(buffer), ocr_pipe) != nullptr) {
          for (char *p = buffer; *p != '\0'; ++p) {
            if (*p == '\x0c') {
              std::string cleaned = clean_text(current_page);
              if (cleaned.size() >= 15) {
                if (cleaned.size() > max_chunk_chars) {
                  cleaned.resize(max_chunk_chars);
                }
                chunks.push_back(ProcessedChunk{std::move(cleaned), page_num, {}});
                if (chunks.size() >= max_chunks_per_file) {
                  pclose(ocr_pipe);
                  return chunks;
                }
              }
              current_page.clear();
              page_num++;
            }
            else {
              current_page.push_back(*p);
            }
          }
        }
        pclose(ocr_pipe);

        if (!current_page.empty()) {
          std::string cleaned = clean_text(current_page);
          if (cleaned.size() >= 15 && chunks.size() < max_chunks_per_file) {
            if (cleaned.size() > max_chunk_chars) {
              cleaned.resize(max_chunk_chars);
            }
            chunks.push_back(ProcessedChunk{std::move(cleaned), page_num, {}});
          }
        }
      }
    }
  }

  return chunks;
}

std::vector<ProcessedChunk> DocumentHandler::extract_docx(const std::filesystem::path &path)
{
  const std::string raw = run_unzip(path, "word/document.xml");
  if (raw.empty()) {
    return {};
  }

  const std::string cleaned = clean_xml_text(raw);
  return to_chunks(cleaned, 0, max_chunks_per_file);
}

std::vector<ProcessedChunk> DocumentHandler::extract_xlsx(const std::filesystem::path &path)
{
  const std::string raw = run_unzip(path, "xl/sharedStrings.xml 'xl/worksheets/*.xml'");
  if (raw.empty()) {
    return {};
  }

  const std::string cleaned = clean_xml_text(raw);
  return to_chunks(cleaned, 0, max_chunks_per_file);
}

std::vector<ProcessedChunk> DocumentHandler::extract_pptx(const std::filesystem::path &path)
{
  const std::string raw = run_unzip(path, "'ppt/slides/slide*.xml'");
  if (raw.empty()) {
    return {};
  }

  const std::string cleaned = clean_xml_text(raw);
  return to_chunks(cleaned, 1, max_chunks_per_file);
}

std::vector<ProcessedChunk> DocumentHandler::extract_opendocument(const std::filesystem::path &path)
{
  const std::string raw = run_unzip(path, "content.xml");
  if (raw.empty()) {
    return {};
  }

  const std::string cleaned = clean_xml_text(raw);
  return to_chunks(cleaned, 0, max_chunks_per_file);
}

std::vector<ProcessedChunk> DocumentHandler::extract_epub(const std::filesystem::path &path)
{
  const std::string raw = run_unzip(path, "'*.xhtml' '*.html'");
  if (raw.empty()) {
    return {};
  }

  const std::string cleaned = clean_html_text(raw);
  return to_chunks(cleaned, 0, max_chunks_per_file);
}

std::vector<ProcessedChunk> DocumentHandler::extract_rtf(const std::filesystem::path &path)
{
  std::ifstream file(path);
  if (!file) {
    return {};
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (content.empty()) {
    return {};
  }

  const std::string cleaned = extract_rtf_text(content);
  return to_chunks(cleaned, 0, max_chunks_per_file);
}

} // namespace lazyfinder
