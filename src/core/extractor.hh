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

private:
  static std::vector<std::string> extract_text_file(const std::filesystem::path &path);
  static std::vector<std::string> extract_pdf_file(const std::filesystem::path &path);
  static std::vector<std::string> extract_docx_file(const std::filesystem::path &path);
};

} // namespace lazyfinder
