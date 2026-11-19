#include "results_model.hh"

#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <filesystem>

namespace lazyfinder
{

namespace
{

QString format_display_dir(const std::filesystem::path &p)
{
  std::string dir = p.parent_path().string();
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

  static const QStringList media_exts = {"png",
                                         "jpg",
                                         "jpeg",
                                         "gif",
                                         "svg",
                                         "webp",
                                         "bmp",
                                         "ico",
                                         "mp4",
                                         "mkv",
                                         "webm",
                                         "avi",
                                         "mov",
                                         "mp3",
                                         "wav",
                                         "ogg",
                                         "flac"};

  const QString lower = ext.toLower();
  if (code_exts.contains(lower)) {
    return "code";
  }
  if (doc_exts.contains(lower)) {
    return "doc";
  }
  if (media_exts.contains(lower)) {
    return "media";
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
  if (category == "media") {
    static const QStringList video_exts = {"mp4", "mkv", "webm", "avi", "mov"};
    static const QStringList audio_exts = {"mp3", "wav", "ogg", "flac"};
    if (video_exts.contains(lower)) {
      return "video";
    }
    if (audio_exts.contains(lower)) {
      return "audio";
    }
    return "image";
  }
  return "file";
}

} // namespace

ResultsModel::ResultsModel(QObject *parent) : QAbstractListModel(parent) {}

int ResultsModel::rowCount(const QModelIndex &parent) const
{
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(filtered_results_.size());
}

QVariant ResultsModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(filtered_results_.size())) {
    return {};
  }

  const auto &item = filtered_results_[static_cast<size_t>(index.row())];

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
  all_results_.clear();
  all_results_.reserve(raw_results.size());

  for (const auto &r : raw_results) {
    std::filesystem::path p(r.path);
    DisplayResult dr;
    dr.path = QString::fromStdString(r.path);
    dr.fileName = QString::fromStdString(p.filename().string());
    dr.directory = format_display_dir(p);
    dr.score = QString::number(r.score, 'f', 3);
    dr.snippet = clean_snippet(r.snippet);
    dr.pageNumber = r.page_number;
    dr.thumbnailPath = QString::fromStdString(r.thumbnail_path);

    QFileInfo fi(dr.path);
    dr.category = determine_category(fi.suffix());
    dr.iconName = determine_icon(dr.category, fi.suffix());

    all_results_.push_back(std::move(dr));
  }

  applyFilter();
}

void ResultsModel::setFilter(const QString &filter)
{
  if (current_filter_ == filter) {
    return;
  }
  current_filter_ = filter;
  applyFilter();
}

void ResultsModel::clear()
{
  beginResetModel();
  all_results_.clear();
  filtered_results_.clear();
  endResetModel();
}

QString ResultsModel::pathAt(int index) const
{
  if (index >= 0 && index < static_cast<int>(filtered_results_.size())) {
    return filtered_results_[static_cast<size_t>(index)].path;
  }
  return {};
}

int ResultsModel::pageNumberAt(int index) const
{
  if (index >= 0 && index < static_cast<int>(filtered_results_.size())) {
    return filtered_results_[static_cast<size_t>(index)].pageNumber;
  }
  return 0;
}

void ResultsModel::applyFilter()
{
  beginResetModel();
  filtered_results_.clear();

  for (const auto &item : all_results_) {
    if (current_filter_ == "all" || current_filter_.isEmpty()) {
      filtered_results_.push_back(item);
    }
    else if (current_filter_ == item.category) {
      filtered_results_.push_back(item);
    }
  }

  endResetModel();
}

} // namespace lazyfinder
