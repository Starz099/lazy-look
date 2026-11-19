#include "pipeline/media_handler.hh"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace lazylook
{

namespace
{

constexpr size_t max_chunk_chars = 2500;
constexpr size_t max_chunks_per_file = 25;
constexpr size_t kMaxSubtitleBufferSizeBytes = 5 * 1024 * 1024;
constexpr size_t kMaxMetadataBufferSizeBytes = 1024 * 1024;
constexpr int kAudioWindowSeconds = 30;
constexpr int kAudioHopSeconds = 15;
constexpr size_t kMaxAudioSlices = 60;
constexpr int kThumbnailWidth = 160;
constexpr int kThumbnailHeight = 90;
constexpr int kThumbnailJpegQuality = 3;
constexpr size_t kMinChunksForVideoSampling = 3;
constexpr size_t kMaxKeyframeSamples = 12;

const std::unordered_set<std::string> &media_extensions()
{
  static const std::unordered_set<std::string> exts = {".mp4",
                                                       ".mkv",
                                                       ".webm",
                                                       ".avi",
                                                       ".mov",
                                                       ".flv",
                                                       ".wmv",
                                                       ".mp3",
                                                       ".wav",
                                                       ".ogg",
                                                       ".flac",
                                                       ".m4a",
                                                       ".aac",
                                                       ".opus",
                                                       ".srt",
                                                       ".vtt"};
  return exts;
}

const std::unordered_set<std::string> &video_extensions()
{
  static const std::unordered_set<std::string> exts = {
      ".mp4", ".mkv", ".webm", ".avi", ".mov", ".flv", ".wmv"};
  return exts;
}

const std::unordered_set<std::string> &audio_extensions()
{
  static const std::unordered_set<std::string> exts = {
      ".mp3", ".wav", ".ogg", ".flac", ".m4a", ".aac", ".opus", ".wma"};
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

std::string strip_markup(std::string_view text)
{
  std::string result;
  result.reserve(text.size());
  bool inside_tag = false;
  for (char c : text) {
    if (c == '<') {
      inside_tag = true;
    }
    else if (c == '>') {
      inside_tag = false;
    }
    else if (!inside_tag) {
      result.push_back(c);
    }
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

std::vector<ProcessedChunk> cues_to_chunks(const std::vector<MediaCue> &cues,
                                           const std::filesystem::path &video_path = {})
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
      std::string thumb;
      if (!video_path.empty() && current_first_ts > 0) {
        thumb = MediaHandler::generate_keyframe_thumbnail(video_path, current_first_ts);
      }
      chunks.push_back(ProcessedChunk{std::move(current_chunk), current_first_ts, thumb, {}});
      current_chunk = "[" + cue.badge + "] " + cue.text;
      current_first_ts = cue.seconds;
      if (chunks.size() >= max_chunks_per_file) {
        break;
      }
    }
  }

  if (!current_chunk.empty() && chunks.size() < max_chunks_per_file) {
    std::string thumb;
    if (!video_path.empty() && current_first_ts > 0) {
      thumb = MediaHandler::generate_keyframe_thumbnail(video_path, current_first_ts);
    }
    chunks.push_back(ProcessedChunk{std::move(current_chunk), current_first_ts, thumb, {}});
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

std::string MediaHandler::format_timecode(int seconds)
{
  char badge_buf[32];
  if (seconds >= 3600) {
    snprintf(badge_buf,
             sizeof(badge_buf),
             "%02d:%02d:%02d",
             seconds / 3600,
             (seconds % 3600) / 60,
             seconds % 60);
  }
  else {
    snprintf(badge_buf, sizeof(badge_buf), "%02d:%02d", seconds / 60, seconds % 60);
  }
  return badge_buf;
}

std::filesystem::path MediaHandler::thumbnails_directory()
{
  std::filesystem::path dir;
  const char *xdg_data = std::getenv("XDG_DATA_HOME");
  if (xdg_data && *xdg_data) {
    dir = std::filesystem::path(xdg_data) / "lazylook" / "thumbnails";
  }
  else {
    const char *home = std::getenv("HOME");
    if (home && *home) {
      dir = std::filesystem::path(home) / ".local" / "share" / "lazylook" / "thumbnails";
    }
    else {
      dir = std::filesystem::temp_directory_path() / "lazylook" / "thumbnails";
    }
  }

  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir;
}

std::string MediaHandler::generate_keyframe_thumbnail(const std::filesystem::path &video_path,
                                                      int timestamp_sec)
{
  const auto thumb_dir = thumbnails_directory();
  if (thumb_dir.empty()) {
    return {};
  }

  // Derive unique and persistent hash name from file path + timestamp
  const size_t path_hash = std::hash<std::string>{}(video_path.string());
  char fname_buf[64];
  snprintf(fname_buf, sizeof(fname_buf), "%016zx_%d.jpg", path_hash, timestamp_sec);
  const auto thumb_path = thumb_dir / fname_buf;

  std::error_code ec;
  if (std::filesystem::exists(thumb_path, ec)) {
    return thumb_path.string();
  }

  // Generate keyframe thumbnail via ffmpeg
  const std::string filter =
      "-vf \"scale=" + std::to_string(kThumbnailWidth) + ":" + std::to_string(kThumbnailHeight) +
      ":force_original_aspect_ratio=decrease,pad=" + std::to_string(kThumbnailWidth) + ":" +
      std::to_string(kThumbnailHeight) + ":(ow-iw)/2:(oh-ih)/2:black\"";
  const std::string cmd = "ffmpeg -y -v error -ss " + std::to_string(timestamp_sec) + " -i " +
                          escape_shell_arg(video_path.string()) + " -vframes 1 -q:v " +
                          std::to_string(kThumbnailJpegQuality) + " " + filter + " " +
                          escape_shell_arg(thumb_path.string()) + " 2>/dev/null";

  int ret = std::system(cmd.c_str());
  if (ret == 0 && std::filesystem::exists(thumb_path, ec)) {
    return thumb_path.string();
  }

  return {};
}

std::vector<VideoChapter> MediaHandler::parse_ffmetadata_chapters(std::string_view metadata_text)
{
  std::vector<VideoChapter> chapters;
  std::istringstream stream((std::string(metadata_text)));
  std::string line;

  bool in_chapter = false;
  int64_t start_val = 0;
  int64_t end_val = 0;
  int64_t timebase_den = 1000;
  std::string chapter_title;

  auto flush_chapter = [&]() {
    if (in_chapter) {
      if (chapter_title.empty()) {
        chapter_title = "Chapter " + std::to_string(chapters.size() + 1);
      }
      int start_sec = timebase_den > 0 ? static_cast<int>(start_val / timebase_den)
                                       : static_cast<int>(start_val);
      int end_sec =
          timebase_den > 0 ? static_cast<int>(end_val / timebase_den) : static_cast<int>(end_val);
      chapters.push_back(VideoChapter{start_sec, end_sec, clean_text(chapter_title)});
    }
    in_chapter = false;
    start_val = 0;
    end_val = 0;
    timebase_den = 1000;
    chapter_title.clear();
  };

  while (std::getline(stream, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
      line.pop_back();
    }
    if (line == "[CHAPTER]") {
      flush_chapter();
      in_chapter = true;
      continue;
    }

    if (!in_chapter) {
      continue;
    }

    const size_t eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }

    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);

    if (key == "TIMEBASE") {
      const size_t slash = val.find('/');
      if (slash != std::string::npos) {
        try {
          timebase_den = std::stoll(val.substr(slash + 1));
        }
        catch (...) {
        }
      }
    }
    else if (key == "START") {
      try {
        start_val = std::stoll(val);
      }
      catch (...) {
      }
    }
    else if (key == "END") {
      try {
        end_val = std::stoll(val);
      }
      catch (...) {
      }
    }
    else if (key == "title" || key == "TITLE") {
      chapter_title = val;
    }
  }

  flush_chapter();
  return chapters;
}

std::vector<VideoChapter> MediaHandler::probe_chapters(const std::filesystem::path &path)
{
  const std::string cmd =
      "ffmpeg -i " + escape_shell_arg(path.string()) + " -f ffmetadata - 2>/dev/null";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return {};
  }

  std::string output;
  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
    if (output.size() >= kMaxMetadataBufferSizeBytes) {
      break;
    }
  }
  pclose(pipe);

  if (output.empty()) {
    return {};
  }

  return parse_ffmetadata_chapters(output);
}

std::vector<MediaCue> MediaHandler::parse_subtitles(std::string_view raw)
{
  std::vector<MediaCue> cues;
  const std::string raw_str(raw);
  std::istringstream stream(raw_str);
  std::string line;

  int current_sec = -1;
  std::string current_badge;
  std::string current_text;

  auto flush_cue = [&]() {
    if (current_sec >= 0) {
      std::string cleaned = clean_text(strip_markup(current_text));
      if (!cleaned.empty()) {
        cues.push_back(MediaCue{current_sec, std::move(current_badge), std::move(cleaned)});
      }
    }
    current_sec = -1;
    current_badge.clear();
    current_text.clear();
  };

  while (std::getline(stream, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
      line.pop_back();
    }
    if (line.empty()) {
      flush_cue();
      continue;
    }

    const size_t arrow_pos = line.find("-->");
    if (arrow_pos != std::string::npos) {
      flush_cue();

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
        else {
          continue;
        }
      }
      catch (...) {
        continue;
      }

      current_sec = secs;
      current_badge = format_timecode(secs);
      continue;
    }

    if (current_sec >= 0) {
      // Check if line is subtitle index counter
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

      if (!current_text.empty()) {
        current_text += " ";
      }
      current_text += line;
    }
  }

  flush_cue();
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

  // Probe container and stream details using ffmpeg stderr output
  const std::string cmd = "ffmpeg -i " + escape_shell_arg(path.string()) + " 2>&1";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (pipe) {
    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
      std::string line(buf);

      // Duration format probe
      const size_t dur_pos = line.find("Duration: ");
      if (dur_pos != std::string::npos) {
        std::string dur_str = line.substr(dur_pos + 10, 8);
        int h = 0, m = 0, s = 0;
        if (sscanf(dur_str.c_str(), "%d:%d:%d", &h, &m, &s) == 3) {
          meta.duration_seconds = h * 3600 + m * 60 + s;
          meta.duration_str = format_timecode(meta.duration_seconds);
        }
      }

      // Video stream probe
      const size_t vid_pos = line.find("Video: ");
      if (vid_pos != std::string::npos) {
        size_t comma = line.find(',', vid_pos);
        if (comma != std::string::npos) {
          meta.codec = line.substr(vid_pos + 7, comma - (vid_pos + 7));
        }

        // Look for resolution pattern
        for (size_t i = vid_pos; i + 7 < line.size(); ++i) {
          if (line[i] == 'x' && std::isdigit(static_cast<unsigned char>(line[i - 1])) &&
              std::isdigit(static_cast<unsigned char>(line[i + 1]))) {
            int w = 0, h = 0;
            size_t w_start = i - 1;
            while (w_start > 0 && std::isdigit(static_cast<unsigned char>(line[w_start - 1]))) {
              w_start--;
            }
            if (sscanf(line.c_str() + w_start, "%dx%d", &w, &h) == 2 && w >= 64 && h >= 64) {
              meta.width = w;
              meta.height = h;
              break;
            }
          }
        }
      }

      // Audio stream probe
      const size_t aud_pos = line.find("Audio: ");
      if (aud_pos != std::string::npos && meta.codec.empty()) {
        size_t comma = line.find(',', aud_pos);
        if (comma != std::string::npos) {
          meta.codec = line.substr(aud_pos + 7, comma - (aud_pos + 7));
        }
      }

      // Common metadata tags
      if (line.find("title           :") != std::string::npos ||
          line.find("title           =") != std::string::npos) {
        size_t colon = line.find_first_of(":= ");
        if (colon != std::string::npos) {
          meta.title = clean_text(line.substr(colon + 1));
        }
      }
      if (line.find("artist          :") != std::string::npos) {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
          meta.artist = clean_text(line.substr(colon + 1));
        }
      }
      if (line.find("album           :") != std::string::npos) {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
          meta.album = clean_text(line.substr(colon + 1));
        }
      }
    }
    pclose(pipe);
  }

  return meta;
}

std::vector<AudioSlice> MediaHandler::slice_audio_windows(const std::filesystem::path &path,
                                                          int duration_sec)
{
  (void)path;
  std::vector<AudioSlice> slices;
  if (duration_sec <= 0) {
    return slices;
  }

  // Audio windows with overlapping hop for speech and music analysis
  const int window = kAudioWindowSeconds;
  const int hop = kAudioHopSeconds;
  const size_t max_slices = kMaxAudioSlices;

  for (int s = 0; s < duration_sec && slices.size() < max_slices; s += hop) {
    int end_s = std::min(s + window, duration_sec);
    std::string badge = format_timecode(s);
    std::string desc = "[" + badge + "] Audio Window (" + format_timecode(s) + " - " +
                       format_timecode(end_s) + ")";
    slices.push_back(AudioSlice{s, end_s, std::move(badge), std::move(desc)});
    if (end_s >= duration_sec) {
      break;
    }
  }
  return slices;
}

std::vector<MediaCue> MediaHandler::transcribe_speech(const std::filesystem::path &path)
{
  // Check if whisper-cli or whisper is available on the system PATH
  std::string whisper_cmd;
  if (std::system("which whisper-cli >/dev/null 2>&1") == 0) {
    whisper_cmd = "whisper-cli";
  }
  else if (std::system("which whisper >/dev/null 2>&1") == 0) {
    whisper_cmd = "whisper";
  }

  if (whisper_cmd.empty()) {
    return {};
  }

  // Run transcription with SRT output format to stdout
  const std::string cmd =
      whisper_cmd + " " + escape_shell_arg(path.string()) + " --output_format srt 2>/dev/null";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return {};
  }

  std::string srt_out;
  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    srt_out += buffer;
    if (srt_out.size() > kMaxSubtitleBufferSizeBytes) {
      break;
    }
  }
  pclose(pipe);

  if (srt_out.empty()) {
    return {};
  }

  return parse_subtitles(srt_out);
}

ProcessedFile MediaHandler::process(const std::filesystem::path &path)
{
  ProcessedFile result;
  result.path = path;
  result.category = FileCategory::Media;

  std::string ext = path.extension().string();
  std::transform(
      ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

  const bool is_video = video_extensions().contains(ext);
  const bool is_audio = audio_extensions().contains(ext);

  // Direct subtitle files (.srt, .vtt)
  if (ext == ".srt" || ext == ".vtt") {
    result.chunks = extract_subtitle_file(path);
  }
  else {
    // Check companion subtitle files in same folder (.srt, .vtt, .en.srt, .en.vtt)
    const std::filesystem::path dir = path.parent_path();
    const std::string stem = path.stem().string();

    const std::vector<std::string> sub_candidates = {
        stem + ".srt", stem + ".vtt", stem + ".en.srt", stem + ".en.vtt"};
    for (const auto &sub_name : sub_candidates) {
      const std::filesystem::path sub_path = dir / sub_name;
      if (std::filesystem::exists(sub_path)) {
        std::ifstream file(sub_path);
        if (file) {
          std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
          auto cues = parse_subtitles(content);
          if (!cues.empty()) {
            result.chunks = cues_to_chunks(cues, is_video ? path : std::filesystem::path{});
            break;
          }
        }
      }
    }

    // Check embedded subtitle streams via ffmpeg
    if (result.chunks.empty()) {
      std::string ffmpeg_cmd = "ffmpeg -v error -i " + escape_shell_arg(path.string()) +
                               " -map 0:s:0 -f srt - 2>/dev/null";
      FILE *pipe = popen(ffmpeg_cmd.c_str(), "r");
      if (pipe) {
        std::string srt_out;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
          srt_out += buffer;
          if (srt_out.size() > kMaxSubtitleBufferSizeBytes) {
            break;
          }
        }
        pclose(pipe);

        if (!srt_out.empty()) {
          auto cues = parse_subtitles(srt_out);
          if (!cues.empty()) {
            result.chunks = cues_to_chunks(cues, is_video ? path : std::filesystem::path{});
          }
        }
      }
    }

    // Ingest native container chapters
    auto chapters = probe_chapters(path);
    for (const auto &chap : chapters) {
      if (chap.title.empty()) {
        continue;
      }
      std::string badge = format_timecode(chap.start_seconds);
      std::string thumb;
      if (is_video) {
        thumb = generate_keyframe_thumbnail(path, chap.start_seconds);
      }
      std::string text = "[" + badge + "] Chapter: " + chap.title;
      result.chunks.push_back(
          ProcessedChunk{std::move(text), chap.start_seconds, std::move(thumb), {}});
    }

    // Speech transcription fallback via whisper
    if (result.chunks.empty()) {
      auto speech_cues = transcribe_speech(path);
      if (!speech_cues.empty()) {
        result.chunks = cues_to_chunks(speech_cues, is_video ? path : std::filesystem::path{});
      }
    }

    // Visual keyframe sampling for videos without subtitles or with few chunks
    if (is_video && result.chunks.size() < kMinChunksForVideoSampling) {
      MediaMetadata meta = probe_metadata(path);
      if (meta.duration_seconds > 0) {
        // Calculate sampling intervals based on video duration
        std::vector<int> sample_timestamps;
        if (meta.duration_seconds <= 10) {
          for (int s = 1; s < meta.duration_seconds; s += 2) {
            sample_timestamps.push_back(s);
          }
          if (sample_timestamps.empty()) {
            sample_timestamps.push_back(0);
          }
        }
        else if (meta.duration_seconds <= 60) {
          for (int s = 2; s < meta.duration_seconds; s += 10) {
            sample_timestamps.push_back(s);
          }
        }
        else if (meta.duration_seconds <= 300) {
          for (int s = 15; s < meta.duration_seconds; s += 30) {
            sample_timestamps.push_back(s);
          }
        }
        else if (meta.duration_seconds <= 1800) {
          for (int s = 30;
               s < meta.duration_seconds && sample_timestamps.size() < kMaxKeyframeSamples;
               s += 90) {
            sample_timestamps.push_back(s);
          }
        }
        else {
          for (int s = 60;
               s < meta.duration_seconds && sample_timestamps.size() < kMaxKeyframeSamples;
               s += 180) {
            sample_timestamps.push_back(s);
          }
        }

        for (int s : sample_timestamps) {
          if (s >= meta.duration_seconds) {
            continue;
          }
          std::string badge = format_timecode(s);
          std::string thumb = generate_keyframe_thumbnail(path, s);
          std::string scene_desc = "[" + badge + "] Video Scene";
          if (meta.width > 0 && meta.height > 0) {
            scene_desc += " (" + std::to_string(meta.width) + "x" + std::to_string(meta.height) +
                          " " + meta.codec + ")";
          }
          scene_desc += " at " + badge;
          result.chunks.push_back(ProcessedChunk{std::move(scene_desc), s, std::move(thumb), {}});
        }
      }
    }

    // Timecoded audio windowing for audio recordings, podcasts, and music without subtitles
    if (is_audio && result.chunks.empty()) {
      MediaMetadata meta = probe_metadata(path);
      if (meta.duration_seconds > 0) {
        auto slices = slice_audio_windows(path, meta.duration_seconds);
        for (const auto &slice : slices) {
          std::string text = slice.description;
          if (!meta.title.empty()) {
            text += " • " + meta.title;
          }
          if (!meta.artist.empty()) {
            text += " by " + meta.artist;
          }
          if (!meta.album.empty()) {
            text += " (" + meta.album + ")";
          }
          result.chunks.push_back(ProcessedChunk{std::move(text), slice.start_seconds, "", {}});
        }
      }
    }
  }

  // Metadata fallback if no chunks were extracted
  if (result.chunks.empty()) {
    MediaMetadata meta = probe_metadata(path);
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

    result.chunks.push_back(ProcessedChunk{std::move(text), 0, "", {}});
  }

  // Prepend filename context
  const std::string fn = path.filename().string();
  for (auto &chunk : result.chunks) {
    if (chunk.text != fn && !chunk.text.starts_with(fn)) {
      chunk.text = fn + "\n" + chunk.text;
    }
  }

  return result;
}

} // namespace lazylook
