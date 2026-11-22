#include "results_model.hh"

#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <filesystem>

namespace lazylook
{

namespace
{

QString format_display_dir(const std::filesystem::path &p)
{
  const std::string dir = p.parent_path().string();
  const QString home = QDir::homePath();
  const QString qdir = QString::fromStdString(dir);
  if (qdir.startsWith(home)) {
    return "~" + qdir.mid(home.length());
  }
  return qdir;
}

QString clean_snippet(std::string_view snippet)
{
  const QString qstr = QString::fromUtf8(snippet.data(), static_cast<qsizetype>(snippet.size()));
  QString clean;
  clean.reserve(qstr.size());
  bool last_space = false;
  for (QChar ch : qstr) {
    if (ch == '\n' || ch == '\r' || ch == '\t') {
      if (!last_space && !clean.isEmpty()) {
        clean.append(' ');
        last_space = true;
      }
    }
    else {
      clean.append(ch);
      last_space = ch.isSpace();
    }
  }
  return clean.trimmed();
}

QString determine_category(const QString &ext)
{
  static const QStringList code_exts = {
      "cc",  "cpp",  "c",    "h",   "hh",   "hpp",  "py",    "js",   "ts",
      "rs",  "go",   "java", "sh",  "bash", "json", "toml",  "yaml", "yml",
      "xml", "html", "css",  "sql", "lua",  "zig",  "cmake", "make", "dockerfile"};

  static const QStringList doc_exts = {"txt",
                                       "md",
                                       "markdown",
                                       "pdf",
                                       "doc",
                                       "docx",
                                       "odt",
                                       "rtf",
                                       "csv",
                                       "tsv",
                                       "org",
                                       "rst",
                                       "xlsx",
                                       "xls",
                                       "pptx",
                                       "ppt",
                                       "ods",
                                       "odp",
                                       "epub"};

  static const QStringList image_exts = {
      "png", "jpg", "jpeg", "gif", "svg", "webp", "bmp", "ico", "tiff", "tif"};

  static const QStringList video_exts = {"mp4", "mkv", "webm", "avi", "mov", "flv", "wmv", "m4v"};

  static const QStringList audio_exts = {"mp3", "wav", "ogg", "flac", "m4a", "aac", "opus", "wma"};

  const QString lower = ext.toLower();
  if (code_exts.contains(lower)) {
    return "code";
  }
  if (doc_exts.contains(lower)) {
    return "doc";
  }
  if (image_exts.contains(lower)) {
    return "image";
  }
  if (video_exts.contains(lower)) {
    return "video";
  }
  if (audio_exts.contains(lower)) {
    return "audio";
  }
  return "other";
}

QString determine_icon(const QString &category, const QString &ext)
{
  const QString lower = ext.toLower();
  if (category == "code") {
    return "code";
  }
  if (category == "doc") {
    if (lower == "pdf") {
      return "pdf";
    }
    if (lower == "md" || lower == "markdown") {
      return "markdown";
    }
    return "document";
  }
  if (category == "image") {
    return "image";
  }
  if (category == "video") {
    return "video";
  }
  if (category == "audio") {
    return "audio";
  }
  return "file";
}

constexpr int kScorePrecision = 3;

} // namespace

ResultsModel::ResultsModel(QObject *parent) : QAbstractListModel(parent) {}

int ResultsModel::rowCount(const QModelIndex &parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(results_.size());
}

QVariant ResultsModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(results_.size())) {
    return {};
  }

  const auto &item = results_[static_cast<size_t>(index.row())];

  switch (role) {
    case PathRole:
      return item.path;
    case FileNameRole:
      return item.fileName;
    case DirectoryRole:
      return item.directory;
    case ScoreRole:
      return item.score;
    case SnippetRole:
      return item.snippet;
    case CategoryRole:
      return item.category;
    case IconNameRole:
      return item.iconName;
    case PageNumberRole:
      return item.pageNumber;
    case ThumbnailPathRole:
      return item.thumbnailPath;
    default:
      return {};
  }
}

QHash<int, QByteArray> ResultsModel::roleNames() const
{
  QHash<int, QByteArray> roles;
  roles[PathRole] = "filePath";
  roles[FileNameRole] = "fileName";
  roles[DirectoryRole] = "directory";
  roles[ScoreRole] = "score";
  roles[SnippetRole] = "snippet";
  roles[CategoryRole] = "category";
  roles[IconNameRole] = "iconName";
  roles[PageNumberRole] = "pageNumber";
  roles[ThumbnailPathRole] = "thumbnailPath";
  return roles;
}

void ResultsModel::setResults(const std::vector<SearchResult> &raw_results, const QString &filter)
{
  current_filter_ = filter;

  beginResetModel();
  results_.clear();
  results_.reserve(raw_results.size());

  for (const auto &r : raw_results) {
    if (r.path.empty()) {
      continue;
    }
    std::filesystem::path p(r.path);
    DisplayResult dr;
    dr.path = QString::fromStdString(r.path);
    dr.fileName = QString::fromStdString(p.filename().string());
    dr.directory = format_display_dir(p);
    dr.score = QString::number(r.score, 'f', kScorePrecision);
    dr.snippet = clean_snippet(r.snippet);
    dr.pageNumber = r.page_number;
    dr.thumbnailPath = QString::fromStdString(r.thumbnail_path);

    const QFileInfo fi(dr.path);
    dr.category = determine_category(fi.suffix());
    dr.iconName = determine_icon(dr.category, fi.suffix());

    const bool matches_filter =
        (current_filter_ == "all" || current_filter_.isEmpty() || current_filter_ == dr.category ||
         (current_filter_ == "media" &&
          (dr.category == "image" || dr.category == "video" || dr.category == "audio")));

    if (matches_filter) {
      results_.push_back(std::move(dr));
    }
  }

  endResetModel();
}

void ResultsModel::setFilter(const QString &filter)
{
  current_filter_ = filter;
}

void ResultsModel::clear()
{
  beginResetModel();
  results_.clear();
  endResetModel();
}

QString ResultsModel::pathAt(int index) const
{
  if (index >= 0 && index < static_cast<int>(results_.size())) {
    return results_[static_cast<size_t>(index)].path;
  }
  return {};
}

int ResultsModel::pageNumberAt(int index) const
{
  if (index >= 0 && index < static_cast<int>(results_.size())) {
    return results_[static_cast<size_t>(index)].pageNumber;
  }
  return 0;
}

bool ResultsModel::updateFilePath(const QString &old_path, const QString &new_path)
{
  for (size_t i = 0; i < results_.size(); ++i) {
    if (results_[i].path == old_path) {
      std::filesystem::path p(new_path.toStdString());
      results_[i].path = new_path;
      results_[i].fileName = QString::fromStdString(p.filename().string());
      results_[i].directory = format_display_dir(p);
      const QModelIndex idx = index(static_cast<int>(i));
      emit dataChanged(idx, idx, {PathRole, FileNameRole, DirectoryRole});
      return true;
    }
  }
  return false;
}

} // namespace lazylook
