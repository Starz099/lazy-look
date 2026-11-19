#include "search_controller.hh"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMetaObject>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <chrono>
#include <filesystem>
#include <iostream>

namespace lazyfinder
{

namespace
{

std::string get_default_model_path()
{
  std::filesystem::path base;
  const char *xdg_data = std::getenv("XDG_DATA_HOME");
  if (xdg_data && *xdg_data) {
    base = std::filesystem::path(xdg_data) / "lazyfinder" / "models";
  }
  else {
    const char *home = std::getenv("HOME");
    if (home && *home) {
      base = std::filesystem::path(home) / ".local" / "share" / "lazyfinder" / "models";
    }
  }

  if (!base.empty()) {
    std::error_code ec;
    const auto q8_path = base / "embeddinggemma-2-Q8_0.gguf";
    if (std::filesystem::exists(q8_path, ec)) {
      return q8_path.string();
    }
    const auto bf16_path = base / "embeddinggemma-2-BF16.gguf";
    if (std::filesystem::exists(bf16_path, ec)) {
      return bf16_path.string();
    }
  }

  return "embeddinggemma-2-Q8_0.gguf";
}

} // namespace

SearchController::SearchController(QObject *parent) : QObject(parent)
{
  debounce_timer_.setSingleShot(true);
  debounce_timer_.setInterval(120);
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
  model_sleep_timer_.setInterval(5 * 60 * 1000); // 5 min idle
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
    const std::string mpath = get_default_model_path();
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

  std::cout << "[LazyFinder] Opening file: " << path.toStdString() << " (page: " << pageNumber
            << ")" << std::endl;

  const QString lower = path.toLower();

  // 1. PDF Documents -> jump directly to page if page > 1
  if (pageNumber > 1 && lower.endsWith(".pdf")) {
    const QString papers_bin = QStandardPaths::findExecutable("papers");
    if (!papers_bin.isEmpty() &&
        QProcess::startDetached(papers_bin, {"-i", QString::number(pageNumber), path})) {
      emit fileOpened();
      return;
    }

    const QString evince_bin = QStandardPaths::findExecutable("evince");
    if (!evince_bin.isEmpty() &&
        QProcess::startDetached(evince_bin, {"-i", QString::number(pageNumber), path})) {
      emit fileOpened();
      return;
    }

    const QString okular_bin = QStandardPaths::findExecutable("okular");
    if (!okular_bin.isEmpty() &&
        QProcess::startDetached(okular_bin, {"-p", QString::number(pageNumber), path})) {
      emit fileOpened();
      return;
    }
  }

  // 2. Presentations (.pptx, .odp, .ppt) -> jump to slide if slide > 1
  if (pageNumber > 1 &&
      (lower.endsWith(".pptx") || lower.endsWith(".odp") || lower.endsWith(".ppt"))) {
    const QString lo_bin = QStandardPaths::findExecutable("libreoffice");
    if (!lo_bin.isEmpty() &&
        QProcess::startDetached(lo_bin, {"--show=" + QString::number(pageNumber), path})) {
      emit fileOpened();
      return;
    }
  }

  // 3. Video / Audio / Subtitle media -> jump to timestamp (seconds) if player supports it
  static const QStringList media_exts = {".mp4",
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
  for (const auto &mext : media_exts) {
    if (lower.endsWith(mext)) {
      is_media = true;
      break;
    }
  }

  QString target_path = path;
  if (is_media && (lower.endsWith(".srt") || lower.endsWith(".vtt"))) {
    QFileInfo fi(path);
    QString base = fi.path() + "/" + fi.completeBaseName();
    for (const auto &vext : {".mp4", ".mkv", ".webm", ".avi", ".mov"}) {
      if (QFile::exists(base + vext)) {
        target_path = base + vext;
        break;
      }
    }
  }

  if (pageNumber > 0 && is_media) {
    // 1. Try mpv
    const QString mpv_bin = QStandardPaths::findExecutable("mpv");
    if (!mpv_bin.isEmpty() &&
        QProcess::startDetached(mpv_bin, {"--start=" + QString::number(pageNumber), target_path})) {
      emit fileOpened();
      return;
    }

    // 2. Try VLC
    const QString vlc_bin = QStandardPaths::findExecutable("vlc");
    if (!vlc_bin.isEmpty() &&
        QProcess::startDetached(vlc_bin,
                                {"--start-time", QString::number(pageNumber), target_path})) {
      emit fileOpened();
      return;
    }

    // 3. Try Totem (GNOME Videos)
    const QString totem_bin = QStandardPaths::findExecutable("totem");
    if (!totem_bin.isEmpty() &&
        QProcess::startDetached(
            totem_bin,
            {"--seek", QString::number(static_cast<qint64>(pageNumber) * 1000), target_path})) {
      emit fileOpened();
      return;
    }

    // 4. Try Celluloid (GNOME MPV frontend)
    const QString celluloid_bin = QStandardPaths::findExecutable("celluloid");
    if (!celluloid_bin.isEmpty() &&
        QProcess::startDetached(celluloid_bin,
                                {"--start=" + QString::number(pageNumber), target_path})) {
      emit fileOpened();
      return;
    }

    // 5. Try SMPlayer
    const QString smplayer_bin = QStandardPaths::findExecutable("smplayer");
    if (!smplayer_bin.isEmpty() &&
        QProcess::startDetached(smplayer_bin,
                                {"-start", QString::number(pageNumber), target_path})) {
      emit fileOpened();
      return;
    }

    // 6. Try Haruna (KDE player)
    const QString haruna_bin = QStandardPaths::findExecutable("haruna");
    if (!haruna_bin.isEmpty() &&
        QProcess::startDetached(haruna_bin,
                                {"--start=" + QString::number(pageNumber), target_path})) {
      emit fileOpened();
      return;
    }
  }

  // Universal fallback: open via default desktop environment handler (Showtime, Clapper, Haruna,
  // etc.)
  bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(target_path));
  if (!ok) {
    const QString gio_bin = QStandardPaths::findExecutable("gio");
    if (!gio_bin.isEmpty() && QProcess::startDetached(gio_bin, {"open", target_path})) {
      ok = true;
    }
    else {
      const QString xdg_bin = QStandardPaths::findExecutable("xdg-open");
      if (!xdg_bin.isEmpty() && QProcess::startDetached(xdg_bin, {target_path})) {
        ok = true;
      }
    }
  }
  std::cout << "[LazyFinder] Open file: " << target_path.toStdString() << " ("
            << (ok ? "success" : "failed") << ")" << std::endl;
  emit fileOpened();
}

void SearchController::revealInFolder(const QString &path)
{
  if (path.isEmpty()) {
    return;
  }
  QFileInfo fi(path);
  QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
}

void SearchController::copyPath(const QString &path)
{
  if (path.isEmpty()) {
    return;
  }
  QClipboard *clipboard = QGuiApplication::clipboard();
  if (clipboard) {
    clipboard->setText(path);
  }
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

      const std::string mpath = get_default_model_path();
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
          else if (scanned % 50 == 0 || scanned <= 5) {
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
    const std::string mpath = get_default_model_path();
    if (embedder_.load_model(mpath)) {
      model_loaded_ = true;
      emit modelLoadedChanged();
    }
  }
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

} // namespace lazyfinder
