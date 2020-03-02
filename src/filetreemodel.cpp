#include "filetreemodel.h"
#include "organizercore.h"
#include "filesorigin.h"
#include "util.h"
#include "directoryentry.h"
#include "fileentry.h"
#include "directorystructure.h"
#include <log.h>

using namespace MOBase;

// in mainwindow.cpp
QString UnmanagedModName();


#define trace(f)


// tracks a contiguous range in the model to avoid calling begin*Rows(), etc.
// for every single item that's added/removed
//
class FileTreeModel::Range
{
public:
  // note that file ranges can start from an index higher than 0 if there are
  // directories
  //
  Range(FileTreeModel* model, FileTreeItem& parentItem, int start=0)
    : m_model(model), m_parentItem(parentItem), m_first(-1), m_current(start)
  {
  }

  // includes the current index in the range
  //
  void includeCurrent()
  {
    // just remember the start of the range, m_current will be used in add()
    // or remove() to figure out the actual range
    if (m_first == -1) {
      m_first = m_current;
    }
  }

  // moves to the next row
  //
  void next()
  {
    ++m_current;
  }

  // returns the current row
  //
  int current() const
  {
    return m_current;
  }

  // adds the given items to this range
  //
  void add(FileTreeItem::Children toAdd)
  {
    if (m_first == -1) {
      // nothing to add
      Q_ASSERT(toAdd.empty());
      return;
    }

    const auto last = m_current - 1;
    const auto parentIndex = m_model->indexFromItem(m_parentItem);

    // make sure the number of items is the same as the size of this range
    Q_ASSERT(static_cast<int>(toAdd.size()) == (last - m_first + 1));

    trace(log::debug("Range::add() {} to {}", m_first, last));

    m_model->beginInsertRows(parentIndex, m_first, last);

    m_parentItem.insert(
      std::make_move_iterator(toAdd.begin()),
      std::make_move_iterator(toAdd.end()),
      static_cast<std::size_t>(m_first));

    m_model->endInsertRows();

    // reset
    m_first = -1;
  }

  // removes the item in this range, returns an iterator to first item passed
  // this range once removed, which can be end()
  //
  FileTreeItem::Children::const_iterator remove()
  {
    if (m_first >= 0) {
      const auto last = m_current - 1;
      const auto parentIndex = m_model->indexFromItem(m_parentItem);

      trace(log::debug("Range::remove() {} to {}", m_first, last));

      m_model->beginRemoveRows(parentIndex, m_first, last);

      m_parentItem.remove(
        static_cast<std::size_t>(m_first),
        static_cast<std::size_t>(last - m_first + 1));

      m_model->endRemoveRows();

      m_model->removePendingIcons(parentIndex, m_first, last);

      // adjust current row to account for those that were just removed
      m_current -= (m_current - m_first);

      // reset
      m_first = -1;
    }

    if (m_current >= m_parentItem.children().size()) {
      return m_parentItem.children().end();
    }

    return m_parentItem.children().begin() + m_current + 1;
  }

private:
  FileTreeModel* m_model;
  FileTreeItem& m_parentItem;
  int m_first;
  int m_current;
};


FileTreeItem* getItem(const QModelIndex& index)
{
  return static_cast<FileTreeItem*>(index.internalPointer());
}

void* makeInternalPointer(FileTreeItem* item)
{
  return item;
}


FileTreeModel::FileTreeModel(OrganizerCore& core, QObject* parent) :
  QAbstractItemModel(parent), m_core(core), m_enabled(true),
  m_root(FileTreeItem::createDirectory(this, nullptr, L"", L"")),
  m_flags(NoFlags), m_fullyLoaded(false)
{
  m_root->setExpanded(true);

  connect(&m_iconPendingTimer, &QTimer::timeout, [&]{ updatePendingIcons(); });
}

void FileTreeModel::refresh()
{
  TimeThis tt("FileTreeModel::refresh()");

  m_fullyLoaded = false;
  update(*m_root, *m_core.directoryStructure()->root(), L"");
}

void FileTreeModel::clear()
{
  m_fullyLoaded = false;

  beginResetModel();
  m_root->clear();
  endResetModel();
}

void FileTreeModel::recursiveFetchMore(const QModelIndex& m)
{
  if (canFetchMore(m)) {
    fetchMore(m);
  }

  for (int i=0; i<rowCount(m); ++i) {
    recursiveFetchMore(index(i, 0, m));
  }
}

void FileTreeModel::ensureFullyLoaded()
{
  if (!m_fullyLoaded) {
    TimeThis tt("FileTreeModel:: fully loading for search");
    recursiveFetchMore(QModelIndex());
    m_fullyLoaded = true;
  }
}

bool FileTreeModel::enabled() const
{
  return m_enabled;
}

void FileTreeModel::setEnabled(bool b)
{
  m_enabled = b;
}

const FileTreeModel::SortInfo& FileTreeModel::sortInfo() const
{
  return m_sort;
}

bool FileTreeModel::showArchives() const
{
  return (m_flags.testFlag(Archives) && m_core.getArchiveParsing());
}

QModelIndex FileTreeModel::index(
  int row, int col, const QModelIndex& parentIndex) const
{
  if (auto* parentItem=itemFromIndex(parentIndex)) {
    if (row < 0 || row >= parentItem->children().size()) {
      log::error("row {} out of range for {}", row, parentItem->debugName());
      return {};
    }

    return createIndex(row, col, makeInternalPointer(parentItem));
  }

  log::error("FileTreeModel::index(): parentIndex has no internal pointer");
  return {};
}

QModelIndex FileTreeModel::parent(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return {};
  }

  auto* parentItem = getItem(index);
  if (!parentItem) {
    log::error("FileTreeModel::parent(): no internal pointer");
    return {};
  }

  return indexFromItem(*parentItem);
}

int FileTreeModel::rowCount(const QModelIndex& parent) const
{
  if (auto* item=itemFromIndex(parent)) {
    return static_cast<int>(item->children().size());
  }

  return 0;
}

int FileTreeModel::columnCount(const QModelIndex&) const
{
  return ColumnCount;
}

bool FileTreeModel::hasChildren(const QModelIndex& parent) const
{
  if (auto* item=itemFromIndex(parent)) {
    return item->hasChildren();
  }

  return false;
}

bool FileTreeModel::canFetchMore(const QModelIndex& parent) const
{
  if (!m_enabled) {
    return false;
  }

  if (auto* item=itemFromIndex(parent)) {
    return !item->isLoaded();
  }

  return false;
}

void FileTreeModel::fetchMore(const QModelIndex& parent)
{
  FileTreeItem* item = itemFromIndex(parent);
  if (!item) {
    return;
  }

  const auto path = item->dataRelativeFilePath();

  auto* parentEntry = m_core.directoryStructure()->root()
    ->findSubDirectoryRecursive(path.toStdWString());

  if (!parentEntry) {
    log::error("FileTreeModel::fetchMore(): directory '{}' not found", path);
    return;
  }

  const auto parentPath = item->dataRelativeParentPath();

  update(*item, *parentEntry, parentPath.toStdWString());
}

QVariant FileTreeModel::data(const QModelIndex& index, int role) const
{
  switch (role)
  {
    case Qt::DisplayRole:
    {
      if (auto* item=itemFromIndex(index)) {
        return displayData(item, index.column());
      }

      break;
    }

    case Qt::FontRole:
    {
      if (auto* item=itemFromIndex(index)) {
        return item->font();
      }

      break;
    }

    case Qt::ToolTipRole:
    {
      if (auto* item=itemFromIndex(index)) {
        return makeTooltip(*item);
      }

      return {};
    }

    case Qt::ForegroundRole:
    {
      if (index.column() == 1) {
        if (auto* item=itemFromIndex(index)) {
          if (item->isConflicted()) {
            return QBrush(Qt::red);
          }
        }
      }

      break;
    }

    case Qt::DecorationRole:
    {
      if (index.column() == 0) {
        if (auto* item=itemFromIndex(index)) {
          return makeIcon(*item, index);
        }
      }

      break;
    }
  }

  return {};
}

QVariant FileTreeModel::headerData(int i, Qt::Orientation ori, int role) const
{
  static const std::array<QString, ColumnCount> names = {
    tr("Name"), tr("Mod"), tr("Type"), tr("Size"), tr("Date modified")
  };

  if (role == Qt::DisplayRole) {
    if (i >= 0 && i < static_cast<int>(names.size())) {
      return names[static_cast<std::size_t>(i)];
    }
  }

  return {};
}

Qt::ItemFlags FileTreeModel::flags(const QModelIndex& index) const
{
  auto f = QAbstractItemModel::flags(index);

  if (auto* item=itemFromIndex(index)) {
    if (!item->hasChildren()) {
      f |= Qt::ItemNeverHasChildren;
    }
  }

  return f;
}

void FileTreeModel::sortItem(FileTreeItem& item, bool force)
{
  emit layoutAboutToBeChanged();

  const auto oldList = persistentIndexList();
  std::vector<std::pair<FileTreeItem*, int>> oldItems;

  const auto itemCount = oldList.size();
  oldItems.reserve(static_cast<std::size_t>(itemCount));

  for (int i=0; i<itemCount; ++i) {
    const QModelIndex& index = oldList[i];
    oldItems.push_back({itemFromIndex(index), index.column()});
  }

  item.sort(m_sort.column, m_sort.order, force);

  QModelIndexList newList;
  newList.reserve(itemCount);

  for (int i=0; i<itemCount; ++i) {
    const auto& pair = oldItems[static_cast<std::size_t>(i)];
    newList.append(indexFromItem(*pair.first, pair.second));
  }

  changePersistentIndexList(oldList, newList);

  emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

void FileTreeModel::sort(int column, Qt::SortOrder order)
{
  m_sort.column = column;
  m_sort.order = order;

  sortItem(*m_root, false);
}

FileTreeItem* FileTreeModel::itemFromIndex(const QModelIndex& index) const
{
  if (!index.isValid()) {
    return m_root.get();
  }

  auto* parentItem = getItem(index);
  if (!parentItem) {
    log::error("FileTreeModel::itemFromIndex(): no internal pointer");
    return nullptr;
  }

  if (index.row() < 0 || index.row() >= parentItem->children().size()) {
    log::error(
      "FileeTreeModel::itemFromIndex(): row {} is out of range for {}",
      index.row(), parentItem->debugName());

    return nullptr;
  }

  return parentItem->children()[index.row()].get();
}

QModelIndex FileTreeModel::indexFromItem(FileTreeItem& item, int col) const
{
  auto* parent = item.parent();
  if (!parent) {
    return {};
  }

  const int index = parent->childIndex(item);
  if (index == -1) {
    log::error(
      "FileTreeMode::indexFromItem(): item {} not found in parent",
      item.debugName());

    return {};
  }

  return createIndex(index, col, makeInternalPointer(parent));
}

void FileTreeModel::update(
  FileTreeItem& parentItem, const DirectoryEntry& parentEntry,
  const std::wstring& parentPath)
{
  trace(log::debug("updating {}", parentItem.debugName()));

  auto path = parentPath;
  if (!parentEntry.isTopLevel()) {
    if (!path.empty()) {
      path += L"\\";
    }

    path += parentEntry.getName();
  }

  parentItem.setLoaded(true);

  bool added = false;

  if (updateDirectories(parentItem, path, parentEntry)) {
    added = true;
  }

  if (updateFiles(parentItem, path, parentEntry)) {
    added = true;
  }

  if (added) {
    parentItem.sort(m_sort.column, m_sort.order, true);
  }
}

bool FileTreeModel::updateDirectories(
  FileTreeItem& parentItem, const std::wstring& parentPath,
  const DirectoryEntry& parentEntry)
{
  // removeDisappearingDirectories() will add directories that are in the
  // tree and still on the filesystem to this set; addNewDirectories() will
  // use this to figure out if a directory is new or not
  std::unordered_set<std::wstring_view> seen;

  removeDisappearingDirectories(parentItem, parentEntry, parentPath, seen);
  return addNewDirectories(parentItem, parentEntry, parentPath, seen);
}

void FileTreeModel::removeDisappearingDirectories(
  FileTreeItem& parentItem, const DirectoryEntry& parentEntry,
  const std::wstring& parentPath, std::unordered_set<std::wstring_view>& seen)
{
  auto& children = parentItem.children();
  auto itor = children.begin();

  // keeps track of the contiguous directories that need to be removed to
  // avoid calling beginRemoveRows(), etc. for each item
  Range range(this, parentItem);

  // for each item in this tree item
  while (itor != children.end()) {
    const auto& item = *itor;

    if (!item->isDirectory()) {
      // directories are always first, no point continuing once a file has
      // been seen
      break;
    }

    auto d = parentEntry.findSubDirectory(
      FileKeyView(item->filenameWsLowerCase()));

    if (d) {
      trace(log::debug("dir {} still there", item->filename()));

      // directory is still there
      seen.emplace(d->getName());

      bool currentRemoved = false;

      if (item->areChildrenVisible()) {
        // the item is currently expanded, update it
        update(*item, *d, parentPath);
      }

      if (shouldShowFolder(*d, item.get())) {
        // folder should be left in the list
        if (!item->areChildrenVisible() && item->isLoaded()) {
          if (!d->isEmpty()) {
            // the item is loaded (previously expanded but now collapsed) and
            // has children, mark  it as unloaded so it updates when next
            // expanded
            item->setLoaded(false);
          }
        }
      } else {
        // item wouldn't have any children, prune it
        trace(log::debug("dir {} is empty and pruned", item->filename()));

        range.includeCurrent();
        currentRemoved = true;
        ++itor;
      }

      if (!currentRemoved) {
        // if there were directories before this row that need to be removed,
        // do it now
        itor = range.remove();
      }
    } else {
      // directory is gone from the parent entry
      trace(log::debug("dir {} is gone", item->filename()));

      range.includeCurrent();
      ++itor;
    }

    range.next();
  }

  // remove the last directory range, if any
  range.remove();
}

bool FileTreeModel::addNewDirectories(
  FileTreeItem& parentItem, const DirectoryEntry& parentEntry,
  const std::wstring& parentPath,
  const std::unordered_set<std::wstring_view>& seen)
{
  // keeps track of the contiguous directories that need to be added to
  // avoid calling beginAddRows(), etc. for each item
  Range range(this, parentItem);
  std::vector<FileTreeItem::Ptr> toAdd;
  bool added = false;

  // for each directory on the filesystem
  for (auto&& d : parentEntry.getSubDirectories()) {
    if (seen.contains(d->getName())) {
      // already seen in the parent item

      // if there were directories before this row that need to be added,
      // do it now
      //
      // todo: if the directory was actually removed in
      // removeDisappearingDirectories(), the range doesn't need to be added
      // now and could be extended further
      range.add(std::move(toAdd));
      toAdd.clear();
    } else {
      if (!shouldShowFolder(*d, nullptr)) {
        // this is a new directory, but it doesn't contain anything interesting
        trace(log::debug("new dir {}, empty and pruned", QString::fromStdWString(d->getName())));

        // act as if this directory doesn't exist at all
        continue;
      }

      // this is a new directory
      trace(log::debug("new dir {}", QString::fromStdWString(d->getName())));

      toAdd.push_back(createDirectoryItem(parentItem, parentPath, *d));
      added = true;

      range.includeCurrent();
    }

    range.next();
  }

  // add the last directory range, if any
  range.add(std::move(toAdd));

  return added;
}

bool FileTreeModel::updateFiles(
  FileTreeItem& parentItem, const std::wstring& parentPath,
  const DirectoryEntry& parentEntry)
{
  // removeDisappearingFiles() will add files that are in the tree and still on
  // the filesystem to this set; addNewFiless() will use this to figure out if
  // a file is new or not
  std::unordered_set<FileIndex> seen;

  int firstFileRow = 0;

  removeDisappearingFiles(parentItem, parentEntry, firstFileRow, seen);
  return addNewFiles(parentItem, parentEntry, parentPath, firstFileRow, seen);
}

void FileTreeModel::removeDisappearingFiles(
  FileTreeItem& parentItem, const DirectoryEntry& parentEntry,
  int& firstFileRow, std::unordered_set<FileIndex>& seen)
{
  auto& children = parentItem.children();
  auto itor = children.begin();

  firstFileRow = -1;

  // keeps track of the contiguous directories that need to be removed to
  // avoid calling beginRemoveRows(), etc. for each item
  Range range(this, parentItem);

  // for each item in this tree item
  while (itor != children.end()) {
    const auto& item = *itor;

    if (!item->isDirectory()) {
      if (firstFileRow == -1) {
        firstFileRow = range.current();
      }

      auto f = parentEntry.findFile(item->key());

      if (f && shouldShowFile(*f)) {
        trace(log::debug("file {} still there", item->filename()));

        // file is still there
        seen.emplace(f->getIndex());

        // there's no good way of checking if the origin of a file has changed
        // to avoid updating items because origin IDs are reused after a
        // refresh, so a file can have the same origin ID after a refresh even
        // if it's actually a different origin
        //
        // the origin's name can't even be checked because it could be changed
        // to something that existed before but was a different origin
        //
        // so this needs to be an unconditional update for all items
        updateFileItem(*item, *f);

        // if there were files before this row that need to be removed,
        // do it now
        itor = range.remove();
      } else {
        // file is gone from the parent entry
        trace(log::debug("file {} is gone", item->filename()));

        range.includeCurrent();
        ++itor;
      }
    } else {
      ++itor;
    }

    range.next();
  }

  // remove the last file range, if any
  range.remove();

  if (firstFileRow == -1) {
    firstFileRow = static_cast<int>(children.size());
  }
}

bool FileTreeModel::addNewFiles(
  FileTreeItem& parentItem, const DirectoryEntry& parentEntry,
  const std::wstring& parentPath, const int firstFileRow,
  const std::unordered_set<FileIndex>& seen)
{
  // keeps track of the contiguous files that need to be added to
  // avoid calling beginAddRows(), etc. for each item
  std::vector<FileTreeItem::Ptr> toAdd;
  Range range(this, parentItem, firstFileRow);
  bool added = false;

  // for each directory on the filesystem
  parentEntry.forEachFileIndex([&](auto&& fileIndex) {
    if (seen.contains(fileIndex)) {
      // already seen in the parent item

      // if there were directories before this row that need to be added,
      // do it now
      range.add(std::move(toAdd));
      toAdd.clear();
    } else {
      const auto file = parentEntry.getFileRegister()->getFile(fileIndex);

      if (!file) {
        log::error(
          "FileTreeModel::addNewFiles(): file index {} in path {} not found",
          fileIndex, parentPath);

        return true;
      }

      if (shouldShowFile(*file)) {
        // this is a new file
        trace(log::debug("new file {}", QString::fromStdWString(file->getName())));

        toAdd.push_back(createFileItem(parentItem, parentPath, *file));
        added = true;

        range.includeCurrent();
      } else {
        // this is a new file, but it shouldn't be shown
        trace(log::debug("new file {}, not shown", QString::fromStdWString(file->getName())));
        return true;
      }
    }

    range.next();

    return true;
  });

  // add the last file range, if any
  range.add(std::move(toAdd));

  return added;
}

FileTreeItem::Ptr FileTreeModel::createDirectoryItem(
  FileTreeItem& parentItem, const std::wstring& parentPath,
  const DirectoryEntry& d)
{
  auto item = FileTreeItem::createDirectory(
    this, &parentItem, parentPath, d.getName());

  if (d.isEmpty()) {
    // if this directory is empty, mark the item as loaded so the expand
    // arrow doesn't show
    item->setLoaded(true);
  }

  return item;
}

FileTreeItem::Ptr FileTreeModel::createFileItem(
  FileTreeItem& parentItem, const std::wstring& parentPath,
  const FileEntry& file)
{
  auto item = FileTreeItem::createFile(
    this, &parentItem, parentPath, file.getName());

  updateFileItem(*item, file);

  item->setLoaded(true);

  return item;
}

void FileTreeModel::updateFileItem(
  FileTreeItem& item, const FileEntry& file)
{
  const int originID = file.getOrigin();

  FileTreeItem::Flags flags = FileTreeItem::NoFlags;

  if (file.isFromArchive()) {
    flags |= FileTreeItem::FromArchive;
  }

  if (!file.getAlternatives().empty()) {
    flags |= FileTreeItem::Conflicted;
  }

  item.setOrigin(
    originID, file.getFullPath(), flags, makeModName(file, originID));

  if (auto s=file.getFileSize()) {
    item.setFileSize(*s);
  }

  if (auto s=file.getCompressedFileSize()) {
    item.setCompressedFileSize(*s);
  }
}

bool FileTreeModel::shouldShowFile(const FileEntry& file) const
{
  if (showConflictsOnly() && (file.getAlternatives().size() == 0)) {
    // only conflicts should be shown, but this file is not conflicted
    return false;
  }

  if (!showArchives() && file.isFromArchive()) {
    // files from archives shouldn't be shown, but this file is from an archive
    return false;
  }

  return true;
}

bool FileTreeModel::shouldShowFolder(
  const DirectoryEntry& dir, const FileTreeItem* item) const
{
  bool shouldPrune = m_flags.testFlag(PruneDirectories);

  if (m_core.settings().archiveParsing()) {
    if (!m_flags.testFlag(Archives)) {
      // archive parsing is enabled but the tree shouldn't show archives; this
      // is a bit of a special case for folders because they have to be hidden
      // regardless of the PruneDirectories flag if they only exist in archives
      //
      // note that this test is inaccurate: if a loose folder exists but is
      // empty, and the same folder exists in an archive but is _not_ empty,
      // then it's considered to exist _only_ in an archive and will be pruned
      //
      // if directories are ever made first-class so they can retain their
      // origins, this test can be made more accurate
      shouldPrune = true;
    }
  }

  if (!shouldPrune) {
    // always show folders regardless of their content
    return true;
  }

  if (item) {
    if (item->isLoaded() && item->children().empty()) {
      // item is loaded and has no children; prune it
      return false;
    }
  }

  bool foundFile = false;

  // check all files in this directory, return early if a file should be shown
  dir.forEachFile([&](auto&& f) {
    if (shouldShowFile(f)) {
      foundFile = true;

      // stop
      return false;
    }

    // continue
    return true;
  });

  if (foundFile) {
    return true;
  }

  // recurse into subdirectories
  for (const auto& subdir : dir.getSubDirectories()) {
    if (shouldShowFolder(*subdir, nullptr)) {
      return true;
    }
  }

  return false;
}

QVariant FileTreeModel::displayData(const FileTreeItem* item, int column) const
{
  switch (column)
  {
    case FileName:
    {
      return item->filename();
    }

    case ModName:
    {
      return item->mod();
    }

    case FileType:
    {
      return item->fileType().value_or(QString());
    }

    case FileSize:
    {
      if (item->isDirectory()) {
        return {};
      } else {
        QString fs;

        if (auto n=item->fileSize()) {
          fs = localizedByteSize(*n);
        }

        if (auto n=item->compressedFileSize()) {
          return QString("%1 (%2)").arg(fs).arg(localizedByteSize(*n));
        } else {
          return fs;
        }
      }
    }

    case LastModified:
    {
      if (auto d=item->lastModified()) {
        if (d->isValid()) {
          return d->toString(Qt::SystemLocaleDate);
        }
      }

      return {};
    }

    default:
    {
      return {};
    }
  }
}

std::wstring FileTreeModel::makeModName(
  const FileEntry& file, int originID) const
{
  static const std::wstring Unmanaged = UnmanagedModName().toStdWString();

  const auto* origin = m_core.directoryStructure()->findOriginByID(originID);

  if (!origin) {
    log::error("FileTreeModel::makeModName(): origin {} not found", originID);
    return Unmanaged;
  }

  if (origin->getID() == 0) {
    return Unmanaged;
  }

  std::wstring name = origin->getName();

  const auto& archive = file.getArchive();
  if (!archive.name.empty()) {
    name += L" (" + archive.name + L")";
  }

  return name;
}

QString FileTreeModel::makeTooltip(const FileTreeItem& item) const
{
  auto nowrap = [&](auto&& s) {
    return "<p style=\"white-space: pre; margin: 0; padding: 0;\">" + s + "</p>";
  };

  auto line = [&](auto&& caption, auto&& value) {
    if (value.isEmpty()) {
      return nowrap("<b>" + caption + ":</b>\n");
    } else {
      return nowrap("<b>" + caption + ":</b> " + value.toHtmlEscaped()) + "\n";
    }
  };


  if (item.isDirectory()) {
    return
      line(tr("Directory"), item.filename()) +
      line(tr("Virtual path"), item.virtualPath());
  }


  static const QString ListStart =
    "<ul style=\""
    "margin-left: 20px; "
    "margin-top: 0; "
    "margin-bottom: 0; "
    "padding: 0; "
    "-qt-list-indent: 0;"
    "\">";

  static const QString ListEnd = "</ul>";


  QString s =
    line(tr("Virtual path"), item.virtualPath()) +
    line(tr("Real path"),    item.realPath()) +
    line(tr("From"),         item.mod());


  const auto file = m_core.directoryStructure()->root()->findFileRecursive(
    item.dataRelativeFilePath().toStdWString());

  if (file) {
    const auto& alternatives = file->getAlternatives();
    QStringList list;

    for (auto&& alt : file->getAlternatives()) {
      const auto* origin = m_core.directoryStructure()->findOriginByID(alt.originID);

      if (!origin) {
        log::error("FileTreeModel::makeTooltip(): origin {} not found", alt.originID);
        continue;
      }

      list.push_back(QString::fromStdWString(origin->getName()));
    }

    if (list.size() == 1) {
      s += line(tr("Also in"), list[0]);
    } else if (list.size() >= 2) {
      s += line(tr("Also in"), QString()) + ListStart;

      for (auto&& alt : list) {
        s += "<li>" + alt +"</li>";
      }

      s += ListEnd;
    }
  }

  return s;
}

QVariant FileTreeModel::makeIcon(
  const FileTreeItem& item, const QModelIndex& index) const
{
  if (item.isDirectory()) {
    return m_iconFetcher.genericDirectoryIcon();
  }

  auto v = m_iconFetcher.icon(item.realPath());
  if (!v.isNull()) {
    return v;
  }

  m_iconPending.push_back(index);
  m_iconPendingTimer.start(std::chrono::milliseconds(1));

  return m_iconFetcher.genericFileIcon();
}

void FileTreeModel::updatePendingIcons()
{
  std::vector<QModelIndex> v(std::move(m_iconPending));
  m_iconPending.clear();

  for (auto&& index : v) {
    emit dataChanged(index, index, {Qt::DecorationRole});
  }

  if (m_iconPending.empty()) {
    m_iconPendingTimer.stop();
  }
}

void FileTreeModel::removePendingIcons(
  const QModelIndex& parent, int first, int last)
{
  auto itor = m_iconPending.begin();

  while (itor != m_iconPending.end()) {
    if (itor->parent() == parent) {
      if (itor->row() >= first && itor->row() <= last) {
        itor = m_iconPending.erase(itor);
        continue;
      }
    }

    ++itor;
  }
}
