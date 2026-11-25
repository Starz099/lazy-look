#pragma once

#include "config.hh"
#include "database.hh"
#include "embedder.hh"
#include "indexer.hh"
#include "results_model.hh"
#include "watcher.hh"

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <atomic>
#include <memory>
#include <thread>

namespace lazylook
{

class SearchController : public QObject
{
  Q_OBJECT

  Q_PROPERTY(lazylook::ResultsModel *resultsModel READ resultsModel CONSTANT)
  Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
  Q_PROPERTY(
      QString currentFilter READ currentFilter WRITE setCurrentFilter NOTIFY currentFilterChanged)
  Q_PROPERTY(bool isIndexing READ isIndexing NOTIFY isIndexingChanged)
  Q_PROPERTY(QString indexingStatus READ indexingStatus NOTIFY indexingStatusChanged)
  Q_PROPERTY(qint64 totalFiles READ totalFiles NOTIFY statsChanged)
  Q_PROPERTY(qint64 totalChunks READ totalChunks NOTIFY statsChanged)
  Q_PROPERTY(
      QStringList watchedDirectories READ watchedDirectories NOTIFY watchedDirectoriesChanged)
  Q_PROPERTY(bool modelLoaded READ modelLoaded NOTIFY modelLoadedChanged)
  Q_PROPERTY(bool isModelInstalled READ isModelInstalled NOTIFY modelInstalledChanged)
  Q_PROPERTY(QString modelPath READ modelPath NOTIFY modelPathChanged)
  Q_PROPERTY(bool isDownloadingModel READ isDownloadingModel NOTIFY isDownloadingModelChanged)
  Q_PROPERTY(
      int modelDownloadProgress READ modelDownloadProgress NOTIFY modelDownloadProgressChanged)
  Q_PROPERTY(QString modelDownloadStatus READ modelDownloadStatus NOTIFY modelDownloadStatusChanged)
  Q_PROPERTY(QString globalShortcut READ globalShortcut WRITE setGlobalShortcut NOTIFY
                 globalShortcutChanged)
  Q_PROPERTY(bool autoSync READ autoSync WRITE setAutoSync NOTIFY autoSyncChanged)
  Q_PROPERTY(bool launchOnStartup READ launchOnStartup WRITE setLaunchOnStartup NOTIFY
                 launchOnStartupChanged)
  Q_PROPERTY(bool memorySaver READ memorySaver WRITE setMemorySaver NOTIFY memorySaverChanged)

public:
  explicit SearchController(QObject *parent = nullptr);
  ~SearchController() override;

  [[nodiscard]] ResultsModel *resultsModel() { return &results_model_; }
  [[nodiscard]] QString query() const { return query_; }
  [[nodiscard]] QString currentFilter() const { return current_filter_; }
  [[nodiscard]] bool isIndexing() const { return is_indexing_; }
  [[nodiscard]] QString indexingStatus() const { return indexing_status_; }
  [[nodiscard]] qint64 totalFiles() const { return total_files_; }
  [[nodiscard]] qint64 totalChunks() const { return total_chunks_; }
  [[nodiscard]] QStringList watchedDirectories() const;
  [[nodiscard]] bool modelLoaded() const { return model_loaded_; }
  [[nodiscard]] bool isModelInstalled() const;
  [[nodiscard]] QString modelPath() const;
  [[nodiscard]] bool isDownloadingModel() const { return is_downloading_model_; }
  [[nodiscard]] int modelDownloadProgress() const { return model_download_progress_; }
  [[nodiscard]] QString modelDownloadStatus() const { return model_download_status_; }
  Q_INVOKABLE void startModelDownload();
  Q_INVOKABLE void cancelModelDownload();

  [[nodiscard]] QString globalShortcut() const;
  Q_INVOKABLE void setGlobalShortcut(const QString &shortcut);

  [[nodiscard]] bool autoSync() const;
  Q_INVOKABLE void setAutoSync(bool enabled);

  [[nodiscard]] bool launchOnStartup() const;
  Q_INVOKABLE void setLaunchOnStartup(bool enabled);

  [[nodiscard]] bool memorySaver() const;
  Q_INVOKABLE void setMemorySaver(bool enabled);

  Q_INVOKABLE void setQuery(const QString &q);
  Q_INVOKABLE void setCurrentFilter(const QString &filter);

  Q_INVOKABLE void search(const QString &text);
  Q_INVOKABLE void openFile(const QString &path, int pageNumber = 0);
  Q_INVOKABLE void revealInFolder(const QString &path);
  Q_INVOKABLE void copyPath(const QString &path);

  // --- Feature: OCR / indexed text copy ---
  // Returns the indexed text content for an image file (OCR result or embedded
  // text).  Empty string if no text was indexed for this file.
  Q_INVOKABLE QString getImageText(const QString &path);
  // Copies the indexed text for an image file to the system clipboard.
  Q_INVOKABLE void copyImageText(const QString &path);

  Q_INVOKABLE void addWatchedDirectory(const QString &dir);
  Q_INVOKABLE void removeWatchedDirectory(const QString &dir);
  Q_INVOKABLE void chooseFolderAndAdd();
  Q_INVOKABLE void openModelDirectory();
  Q_INVOKABLE void quitApp();

  Q_INVOKABLE void startSync();
  Q_INVOKABLE void cancelSync();
  Q_INVOKABLE void refreshStats();

signals:
  void queryChanged();
  void currentFilterChanged();
  void isIndexingChanged();
  void indexingStatusChanged();
  void statsChanged();
  void watchedDirectoriesChanged();
  void modelLoadedChanged();
  void modelInstalledChanged();
  void modelPathChanged();
  void isDownloadingModelChanged();
  void modelDownloadProgressChanged();
  void modelDownloadStatusChanged();
  void searchCompleted();
  void fileOpened();
  void globalShortcutChanged();
  void autoSyncChanged();
  void launchOnStartupChanged();
  void memorySaverChanged();

private:
  void executeSearch(const QString &query_text, uint64_t qid);
  void updateStatsInternal();
  void resetModelSleepTimer();
  void onFilesChanged(const std::vector<std::string> &changed_paths);
  void ensureModelLoaded();

  Database db_;
  Embedder embedder_;
  ResultsModel results_model_;
  Watcher watcher_;

  QString query_;
  QString current_filter_ = "all";
  bool is_indexing_ = false;
  QString indexing_status_;
  qint64 total_files_ = 0;
  qint64 total_chunks_ = 0;
  bool model_loaded_ = false;

  QTimer debounce_timer_;
  QTimer model_sleep_timer_;
  std::atomic<uint64_t> query_counter_{0};

  std::thread indexing_thread_;
  std::atomic<bool> cancel_indexing_{false};

  bool is_downloading_model_ = false;
  int model_download_progress_ = 0;
  QString model_download_status_;
  std::unique_ptr<QNetworkAccessManager> net_manager_;
  QPointer<QNetworkReply> download_reply_;
  std::unique_ptr<QFile> download_file_;
};

} // namespace lazylook
