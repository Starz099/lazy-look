#include "pipeline/media_handler.hh"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace lazyfinder
{

namespace
{

constexpr size_t max_chunk_chars = 2500;
constexpr size_t max_chunks_per_file = 25;

const std::unordered_set<std::string> &media_extensions()
{
  static const std::unordered_set<std::string> exts = {".mp4",
                                                       ".mkv",
                                                       ".webm",
                                                       ".avi",
                                                       ".mov",
                                                       ".mp3",
                                                       ".wav",
                                                       ".ogg",
                                                       ".flac",
                                                       ".m4a",
                                                       ".srt",
                                                       ".vtt"};
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

std::vector<ProcessedChunk> cues_to_chunks(const std::vector<MediaCue> &cues)
{
  std::vector<ProcessedChunk> chunks;
  std::string current_chunk;
  int current_first_ts = 0;

  for (const auto &cue : cues) {
    if (cue.text.empty()) {
      continue;
    }

    if (current_chunk.empty()) {
      current_chunk = "[" + cue.badge + "] " + cue.text;
      current_first_ts = cue.seconds;
    }
    else if (current_chunk.size() + cue.text.size() + 20 < max_chunk_chars) {
      current_chunk += " [" + cue.badge + "] " + cue.text;
    }
    else {
      chunks.push_back(ProcessedChunk{std::move(current_chunk), current_first_ts, {}});
      current_chunk = "[" + cue.badge + "] " + cue.text;
      current_first_ts = cue.seconds;
      if (chunks.size() >= max_chunks_per_file) {
        break;
      }
    }
  }

  if (!current_chunk.empty() && chunks.size() < max_chunks_per_file) {
    chunks.push_back(ProcessedChunk{std::move(current_chunk), current_first_ts, {}});
  }
  return chunks;
}

} // namespace

FileCategory MediaHandler::category() const
{
  return FileCategory::Media;
}

std::string_view MediaHandler::name() const
{
  return "MediaHandler";
}

bool MediaHandler::supports(const std::filesystem::path &path) const
{
  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

  return media_extensions().contains(ext);
}

std::vector<MediaCue> MediaHandler::parse_subtitles(std::string_view raw)
{
  std::vector<MediaCue> cues;
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
      cues.push_back(MediaCue{secs, badge_buf, clean_text(cue_text)});
    }
  }

  return cues;
}

std::vector<ProcessedChunk> MediaHandler::extract_subtitle_file(const std::filesystem::path &path)
{
  std::ifstream file(path);
  if (!file) {
    return {};
  }
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (content.empty()) {
    return {};
  }

  const auto cues = parse_subtitles(content);
  return cues_to_chunks(cues);
}

MediaMetadata MediaHandler::probe_metadata(const std::filesystem::path &path)
{
  MediaMetadata meta;
  std::error_code ec;
  meta.file_size = std::filesystem::file_size(path, ec);

  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
  meta.format = ext.size() > 1 ? ext.substr(1) : "";

  // Probe using ffprobe if available
  const std::string cmd = "ffprobe -v error -show_entries "
                          "format=duration,size,bit_rate:format_tags=title,artist,album,genre:"
                          "stream=width,height,codec_name "
                          "-of default=noprint_wrappers=1 " +
                          escape_shell_arg(path.string()) + " 2>/dev/null";

  FILE *pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
      std::string line(buf);
      while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
      }
      const size_t eq = line.find('=');
      if (eq == std::string::npos) {
        continue;
      }
      std::string key = line.substr(0, eq);
      std::string val = line.substr(eq + 1);

      if (key == "TAG:title" || key == "title") {
        meta.title = val;
      }
      else if (key == "TAG:artist" || key == "artist") {
        meta.artist = val;
      }
      else if (key == "TAG:album" || key == "album") {
        meta.album = val;
      }
      else if (key == "duration") {
        try {
          double d = std::stod(val);
          meta.duration_seconds = static_cast<int>(d);
          char dbuf[32];
          if (meta.duration_seconds >= 3600) {
            snprintf(dbuf,
                     sizeof(dbuf),
                     "%02d:%02d:%02d",
                     meta.duration_seconds / 3600,
                     (meta.duration_seconds % 3600) / 60,
                     meta.duration_seconds % 60);
          }
          else {
            snprintf(dbuf,
                     sizeof(dbuf),
                     "%02d:%02d",
                     meta.duration_seconds / 60,
                     meta.duration_seconds % 60);
          }
          meta.duration_str = dbuf;
        }
        catch (...) {
        }
      }
      else if (key == "width") {
        try {
          meta.width = std::stoi(val);
        }
        catch (...) {
        }
      }
      else if (key == "height") {
        try {
          meta.height = std::stoi(val);
        }
        catch (...) {
        }
      }
      else if (key == "codec_name" && meta.codec.empty()) {
        meta.codec = val;
      }
    }
    pclose(pipe);
  }

  return meta;
}

ProcessedFile MediaHandler::process(const std::filesystem::path &path)
{
  ProcessedFile result;
  result.path = path;
  result.category = FileCategory::Media;

  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

  // 1. Direct subtitle files (.srt, .vtt)
  if (ext == ".srt" || ext == ".vtt") {
    result.chunks = extract_subtitle_file(path);
  }
  else {
    // 2. Check companion subtitle files in same folder (.srt, .vtt, .en.srt, .en.vtt)
    const std::filesystem::path dir = path.parent_path();
    const std::string stem = path.stem().string();

    const std::vector<std::string> sub_candidates = {
        stem + ".srt", stem + ".vtt", stem + ".en.srt", stem + ".en.vtt"};
    for (const auto &sub_name : sub_candidates) {
      const std::filesystem::path sub_path = dir / sub_name;
      if (std::filesystem::exists(sub_path)) {
        auto chunks = extract_subtitle_file(sub_path);
        if (!chunks.empty()) {
          result.chunks = std::move(chunks);
          break;
        }
      }
    }

    // 3. Check embedded subtitles via ffmpeg stream extraction
    if (result.chunks.empty()) {
      std::string ffmpeg_cmd = "ffmpeg -v error -i " + escape_shell_arg(path.string()) +
                               " -map 0:s:0 -f srt - 2>/dev/null";
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
          auto cues = parse_subtitles(srt_out);
          if (!cues.empty()) {
            result.chunks = cues_to_chunks(cues);
          }
        }
      }
    }
  }

  // 4. Metadata probe for audio / video details (or fallback when no subtitle cues)
  MediaMetadata meta = probe_metadata(path);
  if (result.chunks.empty()) {
    std::string text = path.filename().string();
    text += "\n[Media] " + meta.format;
    if (meta.width > 0 && meta.height > 0) {
      text += " (" + std::to_string(meta.width) + "x" + std::to_string(meta.height) + ")";
    }
    if (!meta.duration_str.empty()) {
      text += ", duration: " + meta.duration_str;
    }
    if (!meta.title.empty()) {
      text += "\nTitle: " + meta.title;
    }
    if (!meta.artist.empty()) {
      text += " | Artist: " + meta.artist;
    }
    if (!meta.album.empty()) {
      text += " | Album: " + meta.album;
    }
    text += "\nSize: " + format_size(meta.file_size);

    result.chunks.push_back(ProcessedChunk{std::move(text), 0, {}});
  }

  const std::string fn = path.filename().string();
  for (auto &chunk : result.chunks) {
    if (chunk.text != fn && !chunk.text.starts_with(fn)) {
      chunk.text = fn + "\n" + chunk.text;
    }
  }

  return result;
}

} // namespace lazyfinder
