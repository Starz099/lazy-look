#pragma once

#include "pipeline/handler.hh"

#include <filesystem>
#include <string>
#include <vector>

namespace lazyfinder
{

struct ImageMetadata
{
  int width = 0;
  int height = 0;
  int channels = 0;
  std::string format;
  std::string date_time;
  std::string camera_make;
  std::string camera_model;
  std::string software;
  std::string description;
  std::string text_content; // Embedded SVG text or OCR text
  int64_t file_size = 0;
};

class ImageHandler : public IFileHandler
{
public:
  ImageHandler() = default;
  ~ImageHandler() override = default;

  [[nodiscard]] FileCategory category() const override;
  [[nodiscard]] std::string_view name() const override;
  [[nodiscard]] bool supports(const std::filesystem::path &path) const override;
  [[nodiscard]] ProcessedFile process(const std::filesystem::path &path) override;

  static ImageMetadata extract_metadata(const std::filesystem::path &path);
};

} // namespace lazyfinder
