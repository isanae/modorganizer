#ifndef MODORGANIZER_FILETREEMODEL_INCLUDED
#define MODORGANIZER_FILETREEMODEL_INCLUDED

#include "filetreefwd.h"
#include "iconfetcher.h"
#include <unordered_set>

class OrganizerCore;

namespace filetree
{

class Model : public QAbstractItemModel
{
  Q_OBJECT;

public:
  enum Flag
  {
    NoFlags          = 0x00,
    ConflictsOnly    = 0x01,
    Archives         = 0x02,
    PruneDirectories = 0x04
  };

  enum Columns
  {
    FileName = 0,
    ModName,
    FileType,
    FileSize,
    LastModified,

    ColumnCount
  };

  Q_DECLARE_FLAGS(Flags, Flag);

  struct SortInfo
  {
    int column = 0;
    Qt::SortOrder order = Qt::AscendingOrder;
  };


  Model(OrganizerCore& core, std::unique_ptr<Provider> p, QObject* parent=nullptr);

  void setFlags(Flags f)
  {
    m_flags = f;
  }

  Provider* provider() { return m_provider.get(); }

  void refresh();
  void clear();

  bool fullyLoaded() const
  {
    return m_fullyLoaded;
  }

  void ensureFullyLoaded();

  bool enabled() const;
  void setEnabled(bool b);

  const SortInfo& sortInfo() const;

  QModelIndex index(int row, int col, const QModelIndex& parent={}) const override;
  QModelIndex parent(const QModelIndex& index) const override;
  int rowCount(const QModelIndex& parent={}) const override;
  int columnCount(const QModelIndex& parent={}) const override;
  bool hasChildren(const QModelIndex& parent={}) const override;
  bool canFetchMore(const QModelIndex& parent) const override;
  void fetchMore(const QModelIndex& parent) override;
  QVariant data(const QModelIndex& index, int role=Qt::DisplayRole) const override;
  QVariant headerData(int i, Qt::Orientation ori, int role=Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  void sort(int column, Qt::SortOrder order=Qt::AscendingOrder) override;

  Item* itemFromIndex(const QModelIndex& index) const;
  void sortItem(Item& item, bool force);

private:
  class Range;

  OrganizerCore& m_core;
  bool m_enabled;
  std::unique_ptr<Provider> m_provider;
  mutable ItemPtr m_root;
  Flags m_flags;
  mutable IconFetcher m_iconFetcher;
  mutable std::vector<QModelIndex> m_iconPending;
  mutable QTimer m_iconPendingTimer;
  SortInfo m_sort;
  bool m_fullyLoaded;

  // see top of filetreemodel.cpp
  std::vector<Item*> m_removeItems;
  QTimer m_removeTimer;
  std::vector<Item*> m_sortItems;
  QTimer m_sortTimer;


  bool showConflictsOnly() const
  {
    return (m_flags & ConflictsOnly);
  }

  bool showArchives() const;


  // for `forFetching`, see top of filetreemodel.cpp
  void update(
    Item& parentItem, const Directory& parentDir,
    const std::wstring& parentPath, bool forFetching);

  void queueRemoveItem(Item* item);
  void removeItems();

  void queueSortItem(Item* item);
  void sortItems();


  // for `forFetching`, see top of filetreemodel.cpp
  bool updateDirectories(
    Item& parentItem, const std::wstring& path,
    const Directory& parentDir, bool forFetching);

  // for `forFetching`, see top of filetreemodel.cpp
  void removeDisappearingDirectories(
    Item& parentItem, const Directory& parentDir,
    const std::wstring& parentPath, std::unordered_set<std::wstring_view>& seen,
    bool forFetching);

  bool addNewDirectories(
    Item& parentItem, const Directory& parentDir,
    const std::wstring& parentPath,
    const std::unordered_set<std::wstring_view>& seen);


  bool updateFiles(
    Item& parentItem, const std::wstring& path,
    const Directory& parentDir);

  void removeDisappearingFiles(
    Item& parentItem, const Directory& parentDir,
    int& firstFileRow, std::unordered_set<FileIndex>& seen);

  bool addNewFiles(
    Item& parentItem, const Directory& parentDir,
    const std::wstring& parentPath, int firstFileRow,
    const std::unordered_set<FileIndex>& seen);


  ItemPtr createDirectoryItem(
    Item& parentItem, const std::wstring& parentPath,
    const Directory& dir);

  ItemPtr createFileItem(
    Item& parentItem, const std::wstring& parentPath,
    const File& file);

  void updateFileItem(Item& item, const File& file);


  QVariant displayData(const Item* item, int column) const;
  std::wstring makeModName(const File& file, int originID) const;

  void ensureLoaded(Item* item) const;
  void updatePendingIcons();
  void removePendingIcons(const QModelIndex& parent, int first, int last);

  bool shouldShowFile(const File& file) const;
  bool shouldShowFolder(const Directory& dir, const Item* item) const;
  QString makeTooltip(const Item& item) const;
  QVariant makeIcon(const Item& item, const QModelIndex& index) const;

  QModelIndex indexFromItem(Item& item, int col=0) const;
  void recursiveFetchMore(const QModelIndex& m);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Model::Flags);

} // namespace

#endif // MODORGANIZER_FILETREEMODEL_INCLUDED
