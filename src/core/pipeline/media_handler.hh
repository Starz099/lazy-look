#pragma once

#include "pipeline/handler.hh"

#include <filesystem>
#include <string>
#include <vector>

namespace lazyfinder
{

struct MediaCue
{
  int seconds = 0;
  std::string badge;
  std::string text;
};

struct MediaMetadata
{
  std::string format;
  int duration_seconds = 0;
  std::string duration_str;
  std::string title;
  std::string artist;
  std::string album;
  std::string codec;
  int width = 0;
  int height = 0;
  int64_t file_size = 0;
};

class MediaHandler : public IFileHandler
{
public:
  MediaHandler() = default;
  ~MediaHandler() override = default;

  [[nodiscard]] FileCategory category() const override;
  [[nodiscard]] std::string_view name() const override;
  [[nodiscard]] bool supports(const std::filesystem::path &path) const override;
  [[nodiscard]] ProcessedFile process(const std::filesystem::path &path) override;

  static std::vector<MediaCue> parse_subtitles(std::string_view content);
  static std::vector<ProcessedChunk> extract_subtitle_file(const std::filesystem::path &path);
  static MediaMetadata probe_metadata(const std::filesystem::path &path);
};

} // namespace lazyfinder
