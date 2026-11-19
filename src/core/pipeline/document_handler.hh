#pragma once

#include "pipeline/handler.hh"

#include <filesystem>
#include <string>
#include <vector>

namespace lazylook
{

class DocumentHandler : public IFileHandler
{
public:
  DocumentHandler() = default;
  ~DocumentHandler() override = default;

  [[nodiscard]] FileCategory category() const override;
  [[nodiscard]] std::string_view name() const override;
  [[nodiscard]] bool supports(const std::filesystem::path &path) const override;
  [[nodiscard]] ProcessedFile process(const std::filesystem::path &path) override;

  // Format extractors
  static std::vector<ProcessedChunk> extract_text(const std::filesystem::path &path);
  static std::vector<ProcessedChunk> extract_html(const std::filesystem::path &path);
  static std::vector<ProcessedChunk> extract_pdf(const std::filesystem::path &path);
  static std::vector<ProcessedChunk> extract_docx(const std::filesystem::path &path);
  static std::vector<ProcessedChunk> extract_xlsx(const std::filesystem::path &path);
  static std::vector<ProcessedChunk> extract_pptx(const std::filesystem::path &path);
  static std::vector<ProcessedChunk> extract_opendocument(const std::filesystem::path &path);
  static std::vector<ProcessedChunk> extract_epub(const std::filesystem::path &path);
  static std::vector<ProcessedChunk> extract_rtf(const std::filesystem::path &path);
};

} // namespace lazylook
