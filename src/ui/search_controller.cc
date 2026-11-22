#include "search_controller.hh"

#include "desktop_launcher.hh"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMetaObject>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <chrono>
#include <filesystem>
#include <iostream>

namespace lazylook
{

namespace
{

constexpr int kDebounceIntervalMs = 120;
constexpr int kModelIdleTimeoutMs = 5 * 60 * 1000;
constexpr size_t kScanProgressInterval = 50;
constexpr double kBytesPerMegabyte = 1024.0 * 1024.0;

} // namespace

SearchController::SearchController(QObject *parent) : QObject(parent)
{
  debounce_timer_.setSingleShot(true);
  debounce_timer_.setInterval(kDebounceIntervalMs);
  connect(&debounce_timer_, &QTimer::timeout, this, [this]() {
    const QString current_q = query_.trimmed();
    if (current_q.isEmpty()) {
      results_model_.clear();
      return;
    }
    const uint64_t qid = ++query_counter_;
    executeSearch(current_q, qid);
  });

  model_sleep_timer_.setSingleShot(true);
  model_sleep_timer_.setInterval(kModelIdleTimeoutMs);
  connect(&model_sleep_timer_, &QTimer::timeout, this, [this]() {
    if (Config::instance().memory_saver() && !is_indexing_) {
      embedder_.unload_model();
      model_loaded_ = false;
      emit modelLoadedChanged();
    }
  });

  db_.open();
  Config::instance().load();
  updateStatsInternal();

  // Start inotify background watcher if enabled
  if (Config::instance().auto_sync()) {
    watcher_.start(Config::instance().watched_directories(),
                   [this](const std::vector<std::string> &paths) {
                     QMetaObject::invokeMethod(
                         this, [this, paths]() { onFilesChanged(paths); }, Qt::QueuedConnection);
                   });
  }

  // Ensure GNOME custom shortcut is registered
  Config::register_gnome_shortcut(Config::instance().global_shortcut());

  // Asynchronously load embedder weights without blocking UI
  std::thread([this]() {
    const std::string mpath = Config::resolve_model_path();
    if (embedder_.load_model(mpath)) {
      QMetaObject::invokeMethod(
          this,
          [this]() {
            model_loaded_ = true;
            emit modelLoadedChanged();
            resetModelSleepTimer();
          },
          Qt::QueuedConnection);
    }
  }).detach();
}

SearchController::~SearchController()
{
  Config::unregister_gnome_shortcut();
  watcher_.stop();
  cancel_indexing_ = true;
  if (indexing_thread_.joinable()) {
    indexing_thread_.join();
  }
  db_.close();
}

void SearchController::quitApp()
{
  Config::unregister_gnome_shortcut();
  QCoreApplication::quit();
}

void SearchController::setQuery(const QString &q)
{
  if (query_ == q) {
    return;
  }
  query_ = q;
  emit queryChanged();
  debounce_timer_.start();
}

void SearchController::setCurrentFilter(const QString &filter)
{
  if (current_filter_ == filter) {
    return;
  }
  current_filter_ = filter;
  emit currentFilterChanged();
  results_model_.setFilter(filter);
}

void SearchController::search(const QString &text)
{
  setQuery(text);
}

void SearchController::executeSearch(const QString &query_text, uint64_t qid)
{
  resetModelSleepTimer();
  const std::string q_str = query_text.toStdString();
  const QString filter = current_filter_;

  std::thread([this, q_str, qid, filter]() {
    ensureModelLoaded();
    const auto start_time = std::chrono::steady_clock::now();

    std::vector<float> query_emb;
    if (model_loaded_ && filter != "exact") {
      auto parsed = Database::parse_query_category(q_str);
      std::string embed_text = parsed.text.empty() ? q_str : parsed.text;
      if (!embed_text.empty()) {
        query_emb = embedder_.embed_query(embed_text);
      }
    }

    auto results = db_.search_hybrid(query_emb, q_str, 50);

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - start_time)
                             .count();

    std::cout << "[Search] Query: \"" << q_str << "\" (filter: " << filter.toStdString() << ") -> "
              << results.size() << " results in " << elapsed << "ms" << std::endl;

    if (qid != query_counter_.load()) {
      return;
    }

    QMetaObject::invokeMethod(
        this,
        [this, qid, res = std::move(results), filter]() mutable {
          if (qid == query_counter_.load()) {
            results_model_.setResults(res, filter);
            emit searchCompleted();
          }
        },
        Qt::QueuedConnection);
  }).detach();
}

void SearchController::openFile(const QString &path, int pageNumber)
{
  if (path.isEmpty()) {
    return;
  }
  const bool ok = DesktopLauncher::open_file(path, pageNumber);
  if (ok) {
    emit fileOpened();
  }
}

void SearchController::revealInFolder(const QString &path)
{
  DesktopLauncher::reveal_in_folder(path);
}

void SearchController::copyPath(const QString &path)
{
  DesktopLauncher::copy_path(path);
}

// ---------------------------------------------------------------------------
// Feature: OCR / indexed text copy
// ---------------------------------------------------------------------------

QString SearchController::getImageText(const QString &path)
{
  if (path.isEmpty()) {
    return {};
  }
  const std::string text = db_.get_file_text(path.toStdString());
  if (text.empty()) {
    return {};
  }
  const QString full = QString::fromStdString(text);
  const int idx = full.indexOf("\nText: ");
  if (idx >= 0) {
    return full.mid(idx + 7).trimmed();
  }
  const int nl = full.indexOf('\n');
  const QString body = (nl >= 0 ? full.mid(nl + 1) : full).trimmed();
  if (body.startsWith("[Image]")) {
    // Technical metadata without extracted OCR text
    return {};
  }
  return body;
}

void SearchController::copyImageText(const QString &path)
{
  const QString text = getImageText(path);
  if (text.isEmpty()) {
    return;
  }
  DesktopLauncher::copy_to_clipboard(text);
}

// ---------------------------------------------------------------------------
// Feature: Smart rename
// ---------------------------------------------------------------------------

QString SearchController::suggestFileName(const QString &path)
{
  if (path.isEmpty()) {
    return {};
  }
  const std::string raw = db_.get_file_text(path.toStdString());
  if (raw.empty()) {
    return {};
  }
  const QString full = QString::fromStdString(raw);
  QString body;
  const int ocr_idx = full.indexOf("\nText: ");
  if (ocr_idx >= 0) {
    body = full.mid(ocr_idx + 7).trimmed();
  }
  else {
    const int nl = full.indexOf('\n');
    body = (nl >= 0 ? full.mid(nl + 1) : full).trimmed();
    if (body.startsWith("[Image]")) {
      return {};
    }
  }

  // Heuristic: take the first meaningful non-empty line of content (up to 60
  // chars), clean it up into a valid filename slug.
  const QStringList lines = body.split('\n', Qt::SkipEmptyParts);
  QString candidate;
  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();
    if (trimmed.length() >= 4) {
      candidate = trimmed.left(60);
      break;
    }
  }
  if (candidate.isEmpty()) {
    return {};
  }

  // Convert to a safe filename slug: keep alphanumeric, replace spaces/special
  // chars with hyphens, collapse repeated hyphens, strip leading/trailing ones.
  QString slug;
  slug.reserve(candidate.size());
  bool prev_hyphen = false;
  for (QChar ch : candidate) {
    if (ch.isLetterOrNumber()) {
      slug.append(ch.toLower());
      prev_hyphen = false;
    }
    else if (!prev_hyphen && !slug.isEmpty()) {
      slug.append('-');
      prev_hyphen = true;
    }
  }
  while (!slug.isEmpty() && slug.back() == '-') {
    slug.chop(1);
  }
  return slug;
}

bool SearchController::renameFileTo(const QString &oldPath, const QString &newName)
{
  const QString trimmed = newName.trimmed();
  if (oldPath.isEmpty() || trimmed.isEmpty()) {
    return false;
  }
  // Reject directory traversal, special directory names, and path separators.
  if (trimmed.contains('/') || trimmed.contains('\\') || trimmed == "." || trimmed == "..") {
    return false;
  }

  const QFileInfo fi(oldPath);
  const QString dir = fi.absolutePath();
  const QString suffix = fi.suffix().isEmpty() ? QString{} : ('.' + fi.suffix());

  QString newFileName = trimmed;
  if (!suffix.isEmpty() && !trimmed.endsWith(suffix, Qt::CaseInsensitive)) {
    newFileName += suffix;
  }
  const QString newPath = dir + '/' + newFileName;

  // Refuse to overwrite an existing different file.
  if (QFileInfo::exists(newPath) && newPath != oldPath) {
    return false;
  }

  if (newPath == oldPath) {
    return true;
  }

  if (!QFile::rename(oldPath, newPath)) {
    return false;
  }

  // Update the database entry.
  db_.rename_file(oldPath.toStdString(), newPath.toStdString());

  // Update in-memory ResultsModel so UI reflects new name and subsequent actions work!
  results_model_.updateFilePath(oldPath, newPath);
  return true;
}

QStringList SearchController::watchedDirectories() const
{
  QStringList list;
  for (const auto &d : Config::instance().watched_directories()) {
    list.append(QString::fromStdString(d));
  }
  return list;
}

void SearchController::addWatchedDirectory(const QString &dir)
{
  if (dir.isEmpty()) {
    return;
  }
  const std::string dir_str = dir.toStdString();
  std::cout << "[Folder] Adding watched directory: " << dir_str << std::endl;
  if (Config::instance().add_watched_directory(dir_str)) {
    std::cout << "[Folder] Added. Total watched directories: "
              << Config::instance().watched_directories().size() << std::endl;
    watcher_.update_directories(Config::instance().watched_directories());
    emit watchedDirectoriesChanged();

    // Automatically trigger initial indexing of the newly added folder
    startSync();
  }
  else {
    std::cout << "[Folder] Directory already in watched list: " << dir_str << std::endl;
  }
}

void SearchController::removeWatchedDirectory(const QString &dir)
{
  if (dir.isEmpty()) {
    return;
  }
  const std::string dir_str = dir.toStdString();
  std::cout << "[Folder] Removing watched directory: " << dir_str << std::endl;
  Config::instance().remove_watched_directory(dir_str);
  db_.remove_directory(Config::expand_path(dir_str));
  updateStatsInternal();
  watcher_.update_directories(Config::instance().watched_directories());
  std::cout << "[Folder] Removed and database pruned. Remaining files: " << total_files_ << " ("
            << total_chunks_ << " chunks)" << std::endl;
  emit watchedDirectoriesChanged();

  // If query is currently active, re-execute search to reflect removed records
  if (!query_.trimmed().isEmpty()) {
    const uint64_t qid = ++query_counter_;
    executeSearch(query_.trimmed(), qid);
  }
}

void SearchController::chooseFolderAndAdd()
{
  const QString dir = QFileDialog::getExistingDirectory(nullptr,
                                                        "Select Folder to Index",
                                                        QDir::homePath(),
                                                        QFileDialog::ShowDirsOnly |
                                                            QFileDialog::DontResolveSymlinks);
  if (!dir.isEmpty()) {
    addWatchedDirectory(dir);
  }
}

void SearchController::startSync()
{
  if (is_indexing_) {
    std::cout << "[Sync] Synchronization is already in progress." << std::endl;
    return;
  }
  if (indexing_thread_.joinable()) {
    indexing_thread_.join();
  }

  is_indexing_ = true;
  cancel_indexing_ = false;
  indexing_status_ = "Scanning watched folders...";
  emit isIndexingChanged();
  emit indexingStatusChanged();

  std::cout << "\n==========================================" << std::endl;
  std::cout << "[Sync] Starting synchronization..." << std::endl;
  const auto &dirs = Config::instance().watched_directories();
  std::cout << "[Sync] Watched directories (" << dirs.size() << "):" << std::endl;
  for (const auto &d : dirs) {
    std::cout << "  - " << d << " (" << Config::expand_path(d) << ")" << std::endl;
  }

  indexing_thread_ = std::thread([this]() {
    const auto start_time = std::chrono::steady_clock::now();

    if (!embedder_.is_loaded()) {
      std::cout << "[Sync] Embedding model not yet loaded. Loading weights..." << std::endl;
      QMetaObject::invokeMethod(
          this,
          [this]() {
            indexing_status_ = "Loading AI embedding model...";
            emit indexingStatusChanged();
          },
          Qt::QueuedConnection);

      const std::string mpath = Config::resolve_model_path();
      std::cout << "[Sync] Model path: " << mpath << std::endl;
      if (!embedder_.load_model(mpath)) {
        std::cerr << "[Sync] ERROR: Failed to load AI model from: " << mpath << std::endl;
        QMetaObject::invokeMethod(
            this,
            [this]() {
              is_indexing_ = false;
              indexing_status_ = "Failed to load AI model.";
              emit isIndexingChanged();
              emit indexingStatusChanged();
            },
            Qt::QueuedConnection);
        return;
      }
      std::cout << "[Sync] Embedding model loaded successfully." << std::endl;

      QMetaObject::invokeMethod(
          this,
          [this]() {
            model_loaded_ = true;
            emit modelLoadedChanged();
          },
          Qt::QueuedConnection);
    }
    else {
      std::cout << "[Sync] Embedding model already ready." << std::endl;
    }

    Indexer indexer(db_, embedder_);
    indexer.set_cancel_token(&cancel_indexing_);

    for (const auto &d : Config::instance().watched_directories()) {
      if (cancel_indexing_.load()) {
        break;
      }
      indexer.add_watched_directory(Config::expand_path(d));
    }

    std::cout << "[Sync] Scanning files across watched directories..." << std::endl;

    const auto stats = indexer.index_all(
        [this](const std::filesystem::path &p, size_t scanned, size_t indexed, size_t skipped) {
          if (cancel_indexing_.load()) {
            return;
          }
          if (indexed > 0) {
            std::cout << "[Sync] ✓ [Indexed: " << indexed << " | Scanned: " << scanned << "] "
                      << p.filename().string() << std::endl;
          }
          else if (scanned % kScanProgressInterval == 0 || scanned <= 5) {
            std::cout << "[Sync] Checked " << scanned << " files (" << skipped << " up-to-date)..."
                      << std::endl;
          }

          const QString msg = QString("Indexing %1 (%2 indexed, %3 checked)...")
                                  .arg(QString::fromStdString(p.filename().string()))
                                  .arg(indexed)
                                  .arg(scanned);
          QMetaObject::invokeMethod(
              this,
              [this, msg]() {
                if (is_indexing_ && !cancel_indexing_.load()) {
                  indexing_status_ = msg;
                  emit indexingStatusChanged();
                  updateStatsInternal();
                }
              },
              Qt::QueuedConnection);
        },
        [this](const std::filesystem::path &p, size_t chunk_idx, size_t total_chunks) {
          if (cancel_indexing_.load()) {
            return;
          }
          if (chunk_idx == 0) {
            std::cout << "[Sync] -> Indexing " << p.filename().string() << " (" << total_chunks
                      << " chunks)..." << std::endl;
          }
          else {
            std::cout << "[Sync]    chunk [" << chunk_idx << "/" << total_chunks << "] embedded"
                      << std::endl;
          }

          const QString msg = QString("Embedding %1: chunk %2 of %3...")
                                  .arg(QString::fromStdString(p.filename().string()))
                                  .arg(chunk_idx)
                                  .arg(total_chunks);
          QMetaObject::invokeMethod(
              this,
              [this, msg, chunk_idx]() {
                if (is_indexing_ && !cancel_indexing_.load()) {
                  indexing_status_ = msg;
                  emit indexingStatusChanged();
                  if (chunk_idx == 0) {
                    updateStatsInternal();
                  }
                }
              },
              Qt::QueuedConnection);
        });

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - start_time)
                             .count();

    if (cancel_indexing_.load()) {
      std::cout << "[Sync] Synchronization CANCELLED by user after " << elapsed << "ms."
                << std::endl;
      std::cout << "==========================================\n" << std::endl;
    }
    else {
      std::cout << "[Sync] Synchronization finished in " << (elapsed / 1000.0) << "s." << std::endl;
      std::cout << "  - Total scanned: " << stats.scanned << " files" << std::endl;
      std::cout << "  - Newly indexed: " << stats.indexed << " files" << std::endl;
      std::cout << "  - Up-to-date (skipped): " << stats.skipped << " files" << std::endl;
      std::cout << "  - Errors: " << stats.errors << std::endl;
      std::cout << "  - Total database files: " << db_.total_files() << " (" << db_.total_chunks()
                << " chunks)" << std::endl;
      std::cout << "==========================================\n" << std::endl;
    }

    QMetaObject::invokeMethod(
        this,
        [this, stats]() {
          is_indexing_ = false;
          if (cancel_indexing_.load()) {
            indexing_status_ = "Indexing cancelled.";
          }
          else {
            if (stats.indexed > 0) {
              indexing_status_ = QString("Sync complete. %1 indexed, %2 up-to-date.")
                                     .arg(stats.indexed)
                                     .arg(stats.skipped);
            }
            else {
              indexing_status_ = QString("All %1 files up-to-date.").arg(stats.scanned);
            }
          }
          updateStatsInternal();
          emit isIndexingChanged();
          emit indexingStatusChanged();

          if (!query_.trimmed().isEmpty()) {
            const uint64_t qid = ++query_counter_;
            executeSearch(query_.trimmed(), qid);
          }
        },
        Qt::QueuedConnection);
  });
}

void SearchController::cancelSync()
{
  std::cout << "[Sync] Cancel requested by user." << std::endl;
  cancel_indexing_ = true;
  indexing_status_ = "Cancelling...";
  emit indexingStatusChanged();
}

void SearchController::refreshStats()
{
  updateStatsInternal();
}

void SearchController::updateStatsInternal()
{
  total_files_ = db_.total_files();
  total_chunks_ = db_.total_chunks();
  emit statsChanged();
}

void SearchController::resetModelSleepTimer()
{
  if (Config::instance().memory_saver()) {
    model_sleep_timer_.start();
  }
}

void SearchController::ensureModelLoaded()
{
  if (!embedder_.is_loaded()) {
    const std::string mpath = Config::resolve_model_path();
    if (embedder_.load_model(mpath)) {
      model_loaded_ = true;
      emit modelLoadedChanged();
    }
  }
}

bool SearchController::isModelInstalled() const
{
  return Config::is_model_installed();
}

QString SearchController::modelPath() const
{
  return QString::fromStdString(Config::resolve_model_path());
}

void SearchController::openModelDirectory()
{
  const std::string mpath = Config::resolve_model_path();
  std::filesystem::path dir = std::filesystem::path(mpath).parent_path();
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec)) {
    std::filesystem::create_directories(dir, ec);
  }
  QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(dir.string())));
}

void SearchController::onFilesChanged(const std::vector<std::string> &changed_paths)
{
  if (is_indexing_ || changed_paths.empty()) {
    return;
  }
  std::cout << "[Watcher] Detected " << changed_paths.size() << " changed file(s). Auto-indexing..."
            << std::endl;

  std::thread([this, changed_paths]() {
    ensureModelLoaded();
    Indexer indexer(db_, embedder_);
    int count = 0;
    for (const auto &p_str : changed_paths) {
      std::filesystem::path p(p_str);
      std::error_code ec;
      if (!std::filesystem::exists(p, ec)) {
        db_.remove_file(p_str);
        count++;
      }
      else if (std::filesystem::is_regular_file(p, ec)) {
        if (indexer.index_file(p)) {
          count++;
        }
      }
    }
    QMetaObject::invokeMethod(
        this,
        [this, count]() {
          updateStatsInternal();
          std::cout << "[Watcher] Auto-sync finished: " << count << " record(s) updated."
                    << std::endl;
          resetModelSleepTimer();
        },
        Qt::QueuedConnection);
  }).detach();
}

QString SearchController::globalShortcut() const
{
  return QString::fromStdString(Config::instance().global_shortcut());
}

void SearchController::setGlobalShortcut(const QString &shortcut)
{
  if (shortcut.trimmed().isEmpty()) {
    return;
  }
  Config::instance().set_global_shortcut(shortcut.trimmed().toStdString());
  emit globalShortcutChanged();
}

bool SearchController::autoSync() const
{
  return Config::instance().auto_sync();
}

void SearchController::setAutoSync(bool enabled)
{
  if (Config::instance().auto_sync() == enabled) {
    return;
  }
  Config::instance().set_auto_sync(enabled);
  if (enabled) {
    watcher_.start(Config::instance().watched_directories(),
                   [this](const std::vector<std::string> &paths) {
                     QMetaObject::invokeMethod(
                         this, [this, paths]() { onFilesChanged(paths); }, Qt::QueuedConnection);
                   });
  }
  else {
    watcher_.stop();
  }
  emit autoSyncChanged();
}

bool SearchController::launchOnStartup() const
{
  return Config::instance().launch_on_startup();
}

void SearchController::setLaunchOnStartup(bool enabled)
{
  if (Config::instance().launch_on_startup() == enabled) {
    return;
  }
  Config::instance().set_launch_on_startup(enabled);
  emit launchOnStartupChanged();
}

bool SearchController::memorySaver() const
{
  return Config::instance().memory_saver();
}

void SearchController::setMemorySaver(bool enabled)
{
  if (Config::instance().memory_saver() == enabled) {
    return;
  }
  Config::instance().set_memory_saver(enabled);
  if (enabled) {
    resetModelSleepTimer();
  }
  else {
    model_sleep_timer_.stop();
  }
  emit memorySaverChanged();
}

void SearchController::startModelDownload()
{
  if (is_downloading_model_) {
    return;
  }

  const std::string dest_path = Config::default_user_model_path();
  const std::filesystem::path dest_dir = std::filesystem::path(dest_path).parent_path();
  std::error_code ec;
  std::filesystem::create_directories(dest_dir, ec);

  const QString tmp_path = QString::fromStdString(dest_path + ".tmp");
  download_file_ = std::make_unique<QFile>(tmp_path);
  if (!download_file_->open(QIODevice::WriteOnly)) {
    model_download_status_ = "Error: Cannot write to " + tmp_path;
    emit modelDownloadStatusChanged();
    return;
  }

  if (!net_manager_) {
    net_manager_ = std::make_unique<QNetworkAccessManager>(this);
  }

  const QUrl url("https://huggingface.co/ggml-org/embeddinggemma-2-GGUF/resolve/main/"
                 "embeddinggemma-2-Q8_0.gguf");
  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);

  download_reply_ = net_manager_->get(request);
  is_downloading_model_ = true;
  model_download_progress_ = 0;
  model_download_status_ = "Connecting to Hugging Face...";
  emit isDownloadingModelChanged();
  emit modelDownloadProgressChanged();
  emit modelDownloadStatusChanged();

  connect(download_reply_, &QNetworkReply::readyRead, this, [this]() {
    if (download_file_ && download_reply_) {
      download_file_->write(download_reply_->readAll());
    }
  });

  connect(download_reply_,
          &QNetworkReply::downloadProgress,
          this,
          [this](qint64 bytesReceived, qint64 bytesTotal) {
            if (bytesTotal > 0) {
              const int pct = static_cast<int>((bytesReceived * 100) / bytesTotal);
              if (pct != model_download_progress_) {
                model_download_progress_ = pct;
                emit modelDownloadProgressChanged();
              }
              const double mbReceived = static_cast<double>(bytesReceived) / kBytesPerMegabyte;
              const double mbTotal = static_cast<double>(bytesTotal) / kBytesPerMegabyte;
              model_download_status_ = QString("Downloading EmbeddingGemma-2: %1% (%2 MB / %3 MB)")
                                           .arg(pct)
                                           .arg(QString::number(mbReceived, 'f', 1))
                                           .arg(QString::number(mbTotal, 'f', 1));
              emit modelDownloadStatusChanged();
            }
          });

  connect(download_reply_, &QNetworkReply::finished, this, [this, dest_path, tmp_path]() {
    is_downloading_model_ = false;
    emit isDownloadingModelChanged();

    if (!download_reply_) {
      return;
    }

    if (download_reply_->error() != QNetworkReply::NoError) {
      model_download_status_ = "Download failed: " + download_reply_->errorString();
      emit modelDownloadStatusChanged();
      if (download_file_) {
        download_file_->close();
        download_file_->remove();
        download_file_.reset();
      }
      download_reply_->deleteLater();
      return;
    }

    if (download_file_) {
      download_file_->flush();
      download_file_->close();
      download_file_.reset();
    }

    std::error_code rename_ec;
    std::filesystem::rename(tmp_path.toStdString(), dest_path, rename_ec);
    if (rename_ec) {
      model_download_status_ = "Failed to finalize model file.";
      emit modelDownloadStatusChanged();
      download_reply_->deleteLater();
      return;
    }

    model_download_progress_ = 100;
    model_download_status_ = "Model installed. Initializing neural engine...";
    emit modelDownloadProgressChanged();
    emit modelDownloadStatusChanged();
    emit modelInstalledChanged();
    emit modelPathChanged();

    download_reply_->deleteLater();

    // Asynchronously load the newly downloaded model
    std::thread([this, dest_path]() {
      if (embedder_.load_model(dest_path)) {
        QMetaObject::invokeMethod(
            this,
            [this]() {
              model_loaded_ = true;
              model_download_status_ = "AI engine active.";
              emit modelLoadedChanged();
              emit modelDownloadStatusChanged();
              resetModelSleepTimer();
            },
            Qt::QueuedConnection);
      }
    }).detach();
  });
}

void SearchController::cancelModelDownload()
{
  if (download_reply_) {
    download_reply_->abort();
    download_reply_->deleteLater();
  }
  if (download_file_) {
    download_file_->close();
    download_file_->remove();
    download_file_.reset();
  }
  is_downloading_model_ = false;
  model_download_progress_ = 0;
  model_download_status_ = "Download cancelled.";
  emit isDownloadingModelChanged();
  emit modelDownloadProgressChanged();
  emit modelDownloadStatusChanged();
}

} // namespace lazylook
