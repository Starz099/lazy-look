#include "pipeline/image_handler.hh"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_set>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#pragma GCC diagnostic pop

namespace lazyfinder
{

namespace
{

const std::unordered_set<std::string> &image_extensions()
{
  static const std::unordered_set<std::string> exts = {
      ".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif", ".tiff", ".tif", ".svg"};
  return exts;
}

std::string format_size(int64_t bytes)
{
  if (bytes < 1024) {
    return std::to_string(bytes) + " B";
  }
  if (bytes < 1024 * 1024) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    return buf;
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
  return buf;
}

bool parse_webp_dimensions(const std::filesystem::path &path, int &width, int &height)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }

  unsigned char buf[32];
  file.read(reinterpret_cast<char *>(buf), sizeof(buf));
  if (file.gcount() < 30) {
    return false;
  }

  // Check RIFF header: "RIFF" .... "WEBP"
  if (buf[0] != 'R' || buf[1] != 'I' || buf[2] != 'F' || buf[3] != 'F' || buf[8] != 'W' ||
      buf[9] != 'E' || buf[10] != 'B' || buf[11] != 'P') {
    return false;
  }

  // VP8 lossy
  if (buf[12] == 'V' && buf[13] == 'P' && buf[14] == '8' && buf[15] == ' ') {
    if (buf[23] == 0x9d && buf[24] == 0x01 && buf[25] == 0x2a) {
      width = (buf[26] | (buf[27] << 8)) & 0x3fff;
      height = (buf[28] | (buf[29] << 8)) & 0x3fff;
      return true;
    }
  }
  // VP8L lossless
  else if (buf[12] == 'V' && buf[13] == 'P' && buf[14] == '8' && buf[15] == 'L') {
    if (buf[20] == 0x2f) {
      uint32_t b0 = buf[21];
      uint32_t b1 = buf[22];
      uint32_t b2 = buf[23];
      uint32_t b3 = buf[24];
      width = 1 + (b0 | ((b1 & 0x3f) << 8));
      height = 1 + (((b1 >> 6) | (b2 << 2) | ((b3 & 0x0f) << 10)));
      return true;
    }
  }
  // VP8X extended
  else if (buf[12] == 'V' && buf[13] == 'P' && buf[14] == '8' && buf[15] == 'X') {
    width = 1 + (buf[24] | (buf[25] << 8) | (buf[26] << 16));
    height = 1 + (buf[27] | (buf[28] << 8) | (buf[29] << 16));
    return true;
  }

  return false;
}

void parse_svg_info(const std::filesystem::path &path, ImageMetadata &meta)
{
  meta.format = "SVG";
  meta.channels = 4;

  std::ifstream file(path);
  if (!file) {
    return;
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (content.empty()) {
    return;
  }

  // Extract dimensions from width="..." or height="..." or viewBox="..."
  auto extract_attr = [&](std::string_view tag, std::string_view attr) -> std::string {
    const size_t pos = tag.find(attr);
    if (pos == std::string_view::npos) {
      return {};
    }
    const size_t qpos = tag.find_first_of("\"'", pos + attr.size());
    if (qpos == std::string_view::npos) {
      return {};
    }
    const char quote = tag[qpos];
    const size_t qend = tag.find(quote, qpos + 1);
    if (qend == std::string_view::npos) {
      return {};
    }
    return std::string(tag.substr(qpos + 1, qend - qpos - 1));
  };

  const size_t svg_start = content.find("<svg");
  if (svg_start != std::string::npos) {
    const size_t svg_end = content.find('>', svg_start);
    if (svg_end != std::string_view::npos) {
      std::string svg_tag = content.substr(svg_start, svg_end - svg_start + 1);
      std::string w_str = extract_attr(svg_tag, "width=");
      std::string h_str = extract_attr(svg_tag, "height=");
      try {
        if (!w_str.empty()) {
          meta.width = std::stoi(w_str);
        }
        if (!h_str.empty()) {
          meta.height = std::stoi(h_str);
        }
      }
      catch (...) {
      }

      if (meta.width == 0 || meta.height == 0) {
        std::string vb = extract_attr(svg_tag, "viewBox=");
        if (!vb.empty()) {
          std::istringstream vb_stream(vb);
          double min_x = 0;
          double min_y = 0;
          double vb_w = 0;
          double vb_h = 0;
          if (vb_stream >> min_x >> min_y >> vb_w >> vb_h) {
            meta.width = static_cast<int>(vb_w);
            meta.height = static_cast<int>(vb_h);
          }
        }
      }
    }
  }

  // Extract text elements from <text>, <title>, <desc>
  std::string text_acc;
  bool in_text = false;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '<') {
      if (i + 5 < content.size() && content.substr(i + 1, 4) == "text") {
        in_text = true;
      }
      else if (i + 6 < content.size() && content.substr(i + 1, 5) == "/text") {
        in_text = false;
        text_acc.push_back(' ');
      }
      else if (i + 6 < content.size() && content.substr(i + 1, 5) == "title") {
        in_text = true;
      }
      else if (i + 7 < content.size() && content.substr(i + 1, 6) == "/title") {
        in_text = false;
        text_acc.push_back(' ');
      }
      while (i < content.size() && content[i] != '>') {
        i++;
      }
    }
    else if (in_text) {
      if (static_cast<unsigned char>(content[i]) >= 32) {
        text_acc.push_back(content[i]);
      }
      else if (content[i] == '\n' || content[i] == '\t') {
        text_acc.push_back(' ');
      }
    }
  }

  if (!text_acc.empty()) {
    std::string clean;
    bool last_sp = true;
    for (char c : text_acc) {
      if (c == ' ') {
        if (!last_sp) {
          clean.push_back(' ');
          last_sp = true;
        }
      }
      else {
        clean.push_back(c);
        last_sp = false;
      }
    }
    meta.text_content = clean;
  }
}

void parse_jpeg_exif(const std::filesystem::path &path, ImageMetadata &meta)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return;
  }

  // Read first 64 KB of JPEG which holds APP1 (Exif) marker
  std::vector<unsigned char> buf(65536);
  file.read(reinterpret_cast<char *>(buf.data()), buf.size());
  const size_t len = file.gcount();
  if (len < 16) {
    return;
  }

  // Look for APP1 marker 0xFF 0xE1
  for (size_t i = 0; i + 14 < len; ++i) {
    if (buf[i] == 0xFF && buf[i + 1] == 0xE1) {
      // Check Exif header "Exif\0\0"
      if (buf[i + 4] == 'E' && buf[i + 5] == 'x' && buf[i + 6] == 'i' && buf[i + 7] == 'f' &&
          buf[i + 8] == 0 && buf[i + 9] == 0) {
        // Search printable strings within the APP1 segment
        const size_t app1_len = (buf[i + 2] << 8) | buf[i + 3];
        const size_t end = std::min(len, i + 2 + app1_len);

        std::string ascii_acc;
        for (size_t j = i + 10; j < end; ++j) {
          char c = static_cast<char>(buf[j]);
          if (std::isprint(static_cast<unsigned char>(c)) && c != '%') {
            ascii_acc.push_back(c);
          }
          else {
            if (ascii_acc.size() >= 4) {
              if (ascii_acc.find("Canon") != std::string::npos ||
                  ascii_acc.find("Nikon") != std::string::npos ||
                  ascii_acc.find("Sony") != std::string::npos ||
                  ascii_acc.find("Apple") != std::string::npos ||
                  ascii_acc.find("Google") != std::string::npos ||
                  ascii_acc.find("Samsung") != std::string::npos) {
                if (meta.camera_make.empty()) {
                  meta.camera_make = ascii_acc;
                }
              }
              else if (ascii_acc.size() == 19 && ascii_acc[4] == ':' && ascii_acc[7] == ':') {
                if (meta.date_time.empty()) {
                  meta.date_time = ascii_acc;
                }
              }
            }
            ascii_acc.clear();
          }
        }
        break;
      }
    }
  }
}

} // namespace

FileCategory ImageHandler::category() const
{
  return FileCategory::Image;
}

std::string_view ImageHandler::name() const
{
  return "ImageHandler";
}

bool ImageHandler::supports(const std::filesystem::path &path) const
{
  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

  return image_extensions().contains(ext);
}

ImageMetadata ImageHandler::extract_metadata(const std::filesystem::path &path)
{
  ImageMetadata meta;
  std::error_code ec;
  meta.file_size = std::filesystem::file_size(path, ec);

  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

  if (ext == ".svg") {
    parse_svg_info(path, meta);
    return meta;
  }

  if (ext == ".webp") {
    meta.format = "WebP";
    meta.channels = 4;
    parse_webp_dimensions(path, meta.width, meta.height);
    return meta;
  }

  // Use stb_image header parser for JPEG, PNG, BMP, GIF, etc.
  int w = 0;
  int h = 0;
  int comp = 0;
  const std::string path_str = path.string();
  if (stbi_info(path_str.c_str(), &w, &h, &comp)) {
    meta.width = w;
    meta.height = h;
    meta.channels = comp;

    if (ext == ".jpg" || ext == ".jpeg") {
      meta.format = "JPEG";
      parse_jpeg_exif(path, meta);
    }
    else if (ext == ".png") {
      meta.format = "PNG";
    }
    else if (ext == ".bmp") {
      meta.format = "BMP";
    }
    else if (ext == ".gif") {
      meta.format = "GIF";
    }
    else if (ext == ".tiff" || ext == ".tif") {
      meta.format = "TIFF";
    }
    else {
      meta.format = "Image";
    }
  }
  else {
    meta.format = ext.size() > 1 ? ext.substr(1) : "Image";
    std::transform(meta.format.begin(),
                   meta.format.end(),
                   meta.format.begin(),
                   [](unsigned char c) { return std::toupper(c); });
  }

  // If text is not present, attempt OCR on image via Tesseract if available
  if (meta.text_content.empty() && ext != ".svg") {
    static const bool has_tesseract = (std::system("which tesseract >/dev/null 2>&1") == 0);
    if (has_tesseract) {
      std::string escaped = "'";
      for (const char c : path.string()) {
        if (c == '\'') {
          escaped += "'\\''";
        }
        else {
          escaped += c;
        }
      }
      escaped += "'";

      const std::string ocr_cmd = "tesseract " + escaped + " stdout 2>/dev/null";
      FILE *pipe = popen(ocr_cmd.c_str(), "r");
      if (pipe) {
        std::string ocr_res;
        char buf[1024];
        while (fgets(buf, sizeof(buf), pipe) != nullptr) {
          ocr_res += buf;
          if (ocr_res.size() > 2000) {
            break;
          }
        }
        pclose(pipe);

        std::string cleaned;
        for (char c : ocr_res) {
          if (c == '\r' || c == '\n') {
            if (!cleaned.empty() && cleaned.back() != ' ') {
              cleaned += ' ';
            }
          }
          else if (static_cast<unsigned char>(c) >= 32) {
            cleaned += c;
          }
        }
        while (!cleaned.empty() && cleaned.back() == ' ') {
          cleaned.pop_back();
        }
        if (cleaned.size() >= 5) {
          meta.text_content = std::move(cleaned);
        }
      }
    }
  }

  return meta;
}

ProcessedFile ImageHandler::process(const std::filesystem::path &path)
{
  ProcessedFile result;
  result.path = path;
  result.category = FileCategory::Image;

  ImageMetadata meta = extract_metadata(path);

  // Build structured contextual representation for both lexical (FTS5) and vector embedding
  std::string text = path.filename().string();
  text += "\n[Image] ";

  if (!meta.format.empty()) {
    text += meta.format + " ";
  }

  if (meta.width > 0 && meta.height > 0) {
    text += std::to_string(meta.width) + "x" + std::to_string(meta.height);
    if (meta.channels > 0) {
      text += " (" + std::to_string(meta.channels) + " channels)";
    }
    text += ", ";
  }

  text += format_size(meta.file_size);

  if (!meta.camera_make.empty()) {
    text += "\nCamera: " + meta.camera_make;
  }
  if (!meta.date_time.empty()) {
    text += " | Date: " + meta.date_time;
  }
  if (!meta.text_content.empty()) {
    text += "\nText: " + meta.text_content;
  }

  result.chunks.push_back(ProcessedChunk{std::move(text), 0, {}});
  return result;
}

} // namespace lazyfinder
