#include "desktop_launcher.hh"

#include <QClipboard>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <iostream>

namespace lazylook
{

namespace
{

constexpr int kDBusReplyTimeoutMs = 800;

bool try_launch(const QString &executable, const QStringList &args)
{
  const QString bin = QStandardPaths::findExecutable(executable);
  if (bin.isEmpty()) {
    return false;
  }
  return QProcess::startDetached(bin, args);
}

bool launch_pdf(const QString &path, int page)
{
  if (page <= 1) {
    return false;
  }
  const QString page_str = QString::number(page);
  if (try_launch("papers", {"-i", page_str, path})) {
    return true;
  }
  if (try_launch("evince", {"-i", page_str, path})) {
    return true;
  }
  if (try_launch("okular", {"-p", page_str, path})) {
    return true;
  }
  return false;
}

bool launch_presentation(const QString &path, int slide)
{
  if (slide <= 1) {
    return false;
  }
  return try_launch("libreoffice", {"--show=" + QString::number(slide), path});
}

QString resolve_target_media_path(const QString &path, const QString &lower)
{
  if (lower.endsWith(".srt") || lower.endsWith(".vtt")) {
    const QFileInfo fi(path);
    const QString base = fi.path() + "/" + fi.completeBaseName();
    for (const auto &ext : {".mp4", ".mkv", ".webm", ".avi", ".mov"}) {
      if (QFile::exists(base + ext)) {
        return base + ext;
      }
    }
  }
  return path;
}

bool launch_media(const QString &path, int seconds)
{
  if (seconds <= 0) {
    return false;
  }

  const QString sec_str = QString::number(seconds);
  if (try_launch("mpv", {"--start=" + sec_str, path})) {
    return true;
  }
  if (try_launch("vlc", {"--start-time", sec_str, path})) {
    return true;
  }
  if (try_launch("totem", {"--seek", QString::number(static_cast<qint64>(seconds) * 1000), path})) {
    return true;
  }
  if (try_launch("celluloid", {"--start=" + sec_str, path})) {
    return true;
  }
  if (try_launch("smplayer", {"-start", sec_str, path})) {
    return true;
  }
  if (try_launch("haruna", {"--start=" + sec_str, path})) {
    return true;
  }
  return false;
}

bool launch_desktop_fallback(const QString &path)
{
  if (QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
    return true;
  }
  if (try_launch("gio", {"open", path})) {
    return true;
  }
  return try_launch("xdg-open", {path});
}

} // namespace

bool DesktopLauncher::open_file(const QString &path, int page_or_timestamp)
{
  if (path.isEmpty()) {
    return false;
  }

  std::cout << "[LazyLook] Opening file: " << path.toStdString()
            << " (offset: " << page_or_timestamp << ")" << std::endl;

  const QString lower = path.toLower();

  if (lower.endsWith(".pdf") && page_or_timestamp > 1) {
    if (launch_pdf(path, page_or_timestamp)) {
      return true;
    }
  }

  if ((lower.endsWith(".pptx") || lower.endsWith(".odp") || lower.endsWith(".ppt")) &&
      page_or_timestamp > 1) {
    if (launch_presentation(path, page_or_timestamp)) {
      return true;
    }
  }

  static const QStringList media_extensions = {".mp4",
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

  bool is_media = false;
  for (const auto &ext : media_extensions) {
    if (lower.endsWith(ext)) {
      is_media = true;
      break;
    }
  }

  const QString target_path = is_media ? resolve_target_media_path(path, lower) : path;

  if (is_media && page_or_timestamp > 0) {
    if (launch_media(target_path, page_or_timestamp)) {
      return true;
    }
  }

  const bool success = launch_desktop_fallback(target_path);
  std::cout << "[LazyLook] Open file: " << target_path.toStdString() << " ("
            << (success ? "success" : "failed") << ")" << std::endl;
  return success;
}

bool DesktopLauncher::reveal_in_folder(const QString &path)
{
  if (path.isEmpty()) {
    return false;
  }

  const QFileInfo fi(path);
  if (!fi.exists()) {
    return false;
  }

  const QString file_url = QUrl::fromLocalFile(fi.absoluteFilePath()).toString();
  QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.FileManager1",
                                                    "/org/freedesktop/FileManager1",
                                                    "org.freedesktop.FileManager1",
                                                    "ShowItems");
  msg << QStringList{file_url} << QString("");

  const QDBusMessage reply =
      QDBusConnection::sessionBus().call(msg, QDBus::BlockWithGui, kDBusReplyTimeoutMs);
  if (reply.type() == QDBusMessage::ReplyMessage) {
    return true;
  }

  return QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
}

void DesktopLauncher::copy_path(const QString &path)
{
  if (path.isEmpty()) {
    return;
  }

  QClipboard *clipboard = QGuiApplication::clipboard();
  if (clipboard) {
    clipboard->setText(path);
  }
}

} // namespace lazylook
