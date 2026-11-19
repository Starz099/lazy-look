#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lazyfinder
{

enum class FileCategory
{
  Document,
  Image,
  Media
};

struct ProcessedChunk
{
  std::string text;
  int page_or_timestamp = 0;
  std::string thumbnail_path;
  std::vector<float> custom_embedding;
};

struct ProcessedFile
{
  std::filesystem::path path;
  FileCategory category = FileCategory::Document;
  std::vector<ProcessedChunk> chunks;
};

} // namespace lazyfinder
