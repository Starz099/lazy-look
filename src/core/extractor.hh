#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace lazyfinder
{

class Extractor
{
public:
  static bool is_supported(const std::filesystem::path &path);
  static std::vector<std::string> extract_chunks(const std::filesystem::path &path);

  // Format-specific extractors (also available for direct testing)
  static std::vector<std::string> extract_text_file(const std::filesystem::path &path);
  static std::vector<std::string> extract_html_file(const std::filesystem::path &path);
  static std::vector<std::string> extract_pdf_file(const std::filesystem::path &path);
  static std::vector<std::string> extract_docx_file(const std::filesystem::path &path);
  static std::vector<std::string> extract_xlsx_file(const std::filesystem::path &path);
  static std::vector<std::string> extract_pptx_file(const std::filesystem::path &path);
  static std::vector<std::string> extract_opendocument_file(const std::filesystem::path &path);
  static std::vector<std::string> extract_epub_file(const std::filesystem::path &path);
  static std::vector<std::string> extract_rtf_file(const std::filesystem::path &path);
};

} // namespace lazyfinder
