#pragma once

#include "config.hh"
#include "database.hh"
#include "embedder.hh"
#include "indexer.hh"
#include "results_model.hh"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <atomic>
#include <memory>
#include <thread>

namespace lazyfinder
{

class SearchController : public QObject
{
  Q_OBJECT

  Q_PROPERTY(lazyfinder::ResultsModel *resultsModel READ resultsModel CONSTANT)
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

  Q_INVOKABLE void setQuery(const QString &q);
  Q_INVOKABLE void setCurrentFilter(const QString &filter);

  Q_INVOKABLE void search(const QString &text);
  Q_INVOKABLE void openFile(const QString &path, int pageNumber = 0);
  Q_INVOKABLE void revealInFolder(const QString &path);
  Q_INVOKABLE void copyPath(const QString &path);

  Q_INVOKABLE void addWatchedDirectory(const QString &dir);
  Q_INVOKABLE void removeWatchedDirectory(const QString &dir);
  Q_INVOKABLE void chooseFolderAndAdd();

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
  void searchCompleted();
  void fileOpened();

private:
  void executeSearch(const QString &query_text, uint64_t qid);
  void updateStatsInternal();

  Database db_;
  Embedder embedder_;
  ResultsModel results_model_;

  QString query_;
  QString current_filter_ = "all";
  bool is_indexing_ = false;
  QString indexing_status_;
  qint64 total_files_ = 0;
  qint64 total_chunks_ = 0;
  bool model_loaded_ = false;

  QTimer debounce_timer_;
  std::atomic<uint64_t> query_counter_{0};

  std::thread indexing_thread_;
  std::atomic<bool> cancel_indexing_{false};
};

} // namespace lazyfinder
