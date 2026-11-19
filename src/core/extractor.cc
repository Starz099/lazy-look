#include "extractor.hh"

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

const std::unordered_set<std::string> &supported_extensions()
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
                                                       ".rtf",
                                                       // Subtitles and Media
                                                       ".srt",
                                                       ".vtt",
                                                       ".mp4",
                                                       ".mkv",
                                                       ".webm",
                                                       ".avi",
                                                       ".mov",
                                                       ".mp3",
                                                       ".wav",
                                                       ".ogg",
                                                       ".flac",
                                                       ".m4a"};
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
          // Trailing delimiter space after control word
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
  if (ec || size == 0) {
    return {};
  }

  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

  static const std::unordered_set<std::string> media_exts = {
      ".mp4", ".mkv", ".webm", ".avi", ".mov", ".mp3", ".wav", ".ogg", ".flac", ".m4a"};
  const bool is_media = media_exts.contains(ext);
  if (!is_media && size > max_file_size) {
    return {};
  }

  std::vector<std::string> chunks;
  if (ext == ".pdf") {
    chunks = extract_pdf_file(path);
  }
  else if (ext == ".docx") {
    chunks = extract_docx_file(path);
  }
  else if (ext == ".xlsx") {
    chunks = extract_xlsx_file(path);
  }
  else if (ext == ".pptx") {
    chunks = extract_pptx_file(path);
  }
  else if (ext == ".odt" || ext == ".ods" || ext == ".odp") {
    chunks = extract_opendocument_file(path);
  }
  else if (ext == ".epub") {
    chunks = extract_epub_file(path);
  }
  else if (ext == ".rtf") {
    chunks = extract_rtf_file(path);
  }
  else if (ext == ".html" || ext == ".htm") {
    chunks = extract_html_file(path);
  }
  else if (ext == ".srt" || ext == ".vtt") {
    chunks = extract_subtitle_file(path);
  }
  else if (is_media) {
    chunks = extract_media_file(path);
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

std::vector<std::string> Extractor::extract_html_file(const std::filesystem::path &path)
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
  return chunk_text(cleaned, max_chunks_per_file);
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
  const std::string raw = run_unzip(path, "word/document.xml");
  if (raw.empty()) {
    return {};
  }

  const std::string cleaned = clean_xml_text(raw);
  return chunk_text(cleaned, max_chunks_per_file);
}

std::vector<std::string> Extractor::extract_xlsx_file(const std::filesystem::path &path)
{
  const std::string raw = run_unzip(path, "xl/sharedStrings.xml 'xl/worksheets/*.xml'");
  if (raw.empty()) {
    return {};
  }

  const std::string cleaned = clean_xml_text(raw);
  return chunk_text(cleaned, max_chunks_per_file);
}

std::vector<std::string> Extractor::extract_pptx_file(const std::filesystem::path &path)
{
  const std::string raw = run_unzip(path, "'ppt/slides/slide*.xml'");
  if (raw.empty()) {
    return {};
  }

  const std::string cleaned = clean_xml_text(raw);
  return chunk_text(cleaned, max_chunks_per_file);
}

std::vector<std::string> Extractor::extract_opendocument_file(const std::filesystem::path &path)
{
  const std::string raw = run_unzip(path, "content.xml");
  if (raw.empty()) {
    return {};
  }

  const std::string cleaned = clean_xml_text(raw);
  return chunk_text(cleaned, max_chunks_per_file);
}

std::vector<std::string> Extractor::extract_epub_file(const std::filesystem::path &path)
{
  const std::string raw = run_unzip(path, "'*.xhtml' '*.html'");
  if (raw.empty()) {
    return {};
  }

  const std::string cleaned = clean_html_text(raw);
  return chunk_text(cleaned, max_chunks_per_file);
}

std::vector<std::string> Extractor::extract_rtf_file(const std::filesystem::path &path)
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
  return chunk_text(cleaned, max_chunks_per_file);
}

namespace
{

struct SubtitleCue
{
  int seconds = 0;
  std::string badge;
  std::string text;
};

std::vector<SubtitleCue> parse_subtitle_text(std::string_view raw)
{
  std::vector<SubtitleCue> cues;
  const std::string raw_str(raw);
  std::istringstream stream(raw_str);
  std::string line;

  while (std::getline(stream, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
      line.pop_back();
    }
    const size_t arrow_pos = line.find("-->");
    if (arrow_pos == std::string::npos) {
      continue;
    }

    std::string start_str = line.substr(0, arrow_pos);
    while (!start_str.empty() && (start_str.front() == ' ' || start_str.front() == '\t')) {
      start_str.erase(start_str.begin());
    }
    while (!start_str.empty() && (start_str.back() == ' ' || start_str.back() == '\t')) {
      start_str.pop_back();
    }
    for (char &c : start_str) {
      if (c == ',') {
        c = '.';
      }
    }

    std::vector<std::string> parts;
    std::istringstream tss(start_str);
    std::string part;
    while (std::getline(tss, part, ':')) {
      parts.push_back(part);
    }

    int secs = 0;
    try {
      if (parts.size() == 3) {
        int h = std::stoi(parts[0]);
        int m = std::stoi(parts[1]);
        double s = std::stod(parts[2]);
        secs = h * 3600 + m * 60 + static_cast<int>(s);
      }
      else if (parts.size() == 2) {
        int m = std::stoi(parts[0]);
        double s = std::stod(parts[1]);
        secs = m * 60 + static_cast<int>(s);
      }
    }
    catch (...) {
      continue;
    }

    char badge_buf[32];
    if (secs >= 3600) {
      snprintf(badge_buf,
               sizeof(badge_buf),
               "%02d:%02d:%02d",
               secs / 3600,
               (secs % 3600) / 60,
               secs % 60);
    }
    else {
      snprintf(badge_buf, sizeof(badge_buf), "%02d:%02d", secs / 60, secs % 60);
    }

    std::string cue_text;
    while (std::getline(stream, line)) {
      while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
        line.pop_back();
      }
      if (line.empty() || line.find("-->") != std::string::npos) {
        break;
      }
      bool is_all_digits = true;
      for (char ch : line) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
          is_all_digits = false;
          break;
        }
      }
      if (is_all_digits) {
        continue;
      }

      if (!cue_text.empty()) {
        cue_text += " ";
      }
      cue_text += line;
    }

    if (!cue_text.empty()) {
      cues.push_back(SubtitleCue{secs, badge_buf, clean_text(cue_text)});
    }
  }

  return cues;
}

std::vector<std::string> cues_to_chunks(const std::vector<SubtitleCue> &cues)
{
  std::vector<std::string> chunks;
  std::string current_chunk;

  for (const auto &cue : cues) {
    if (cue.text.empty()) {
      continue;
    }

    if (current_chunk.empty()) {
      current_chunk = "[" + cue.badge + "] " + cue.text;
    }
    else if (current_chunk.size() + cue.text.size() + 20 < max_chunk_chars) {
      current_chunk += " [" + cue.badge + "] " + cue.text;
    }
    else {
      chunks.push_back(std::move(current_chunk));
      current_chunk = "[" + cue.badge + "] " + cue.text;
      if (chunks.size() >= max_chunks_per_file) {
        break;
      }
    }
  }

  if (!current_chunk.empty() && chunks.size() < max_chunks_per_file) {
    chunks.push_back(std::move(current_chunk));
  }
  return chunks;
}

} // namespace

std::vector<std::string> Extractor::extract_subtitle_file(const std::filesystem::path &path)
{
  std::ifstream file(path);
  if (!file) {
    return {};
  }
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (content.empty()) {
    return {};
  }

  const auto cues = parse_subtitle_text(content);
  return cues_to_chunks(cues);
}

std::vector<std::string> Extractor::extract_media_file(const std::filesystem::path &path)
{
  // 1. Check companion subtitle files in same folder (.srt, .vtt, .en.srt, .en.vtt)
  const std::filesystem::path dir = path.parent_path();
  const std::string stem = path.stem().string();

  const std::vector<std::string> sub_candidates = {
      stem + ".srt", stem + ".vtt", stem + ".en.srt", stem + ".en.vtt"};
  for (const auto &sub_name : sub_candidates) {
    const std::filesystem::path sub_path = dir / sub_name;
    if (std::filesystem::exists(sub_path)) {
      auto chunks = extract_subtitle_file(sub_path);
      if (!chunks.empty()) {
        return chunks;
      }
    }
  }

  // 2. Check embedded subtitles via ffmpeg stream extraction
  std::string ffmpeg_cmd =
      "ffmpeg -v error -i " + escape_shell_arg(path.string()) + " -map 0:s:0 -f srt - 2>/dev/null";
  FILE *pipe = popen(ffmpeg_cmd.c_str(), "r");
  if (pipe) {
    std::string srt_out;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      srt_out += buffer;
      if (srt_out.size() > 5 * 1024 * 1024) {
        break;
      }
    }
    pclose(pipe);

    if (!srt_out.empty()) {
      auto cues = parse_subtitle_text(srt_out);
      if (!cues.empty()) {
        return cues_to_chunks(cues);
      }
    }
  }

  // 3. Fallback: index the filename
  return {path.filename().string()};
}

} // namespace lazyfinder
