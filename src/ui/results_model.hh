#pragma once

#include "database.hh"

#include <QAbstractListModel>
#include <QString>
#include <vector>

namespace lazylook
{

struct DisplayResult
{
  QString path;
  QString fileName;
  QString directory;
  QString score;
  QString snippet;
  QString category;
  QString iconName;
  int pageNumber = 0;
  QString thumbnailPath;
};

class ResultsModel : public QAbstractListModel
{
  Q_OBJECT

public:
  enum ResultRoles
  {
    PathRole = Qt::UserRole + 1,
    FileNameRole,
    DirectoryRole,
    ScoreRole,
    SnippetRole,
    CategoryRole,
    IconNameRole,
    PageNumberRole,
    ThumbnailPathRole
  };

  explicit ResultsModel(QObject *parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void setResults(const std::vector<SearchResult> &raw_results, const QString &filter = "all");
  void setFilter(const QString &filter);
  void clear();

  [[nodiscard]] Q_INVOKABLE QString pathAt(int index) const;
  [[nodiscard]] Q_INVOKABLE int pageNumberAt(int index) const;

private:
  std::vector<DisplayResult> results_;
  QString current_filter_ = "all";
};

} // namespace lazylook
