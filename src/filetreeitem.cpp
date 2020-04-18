#include "filetreeitem.h"
#include "filetreemodel.h"
#include "modinfo.h"
#include "util.h"
#include "modinfodialogfwd.h"
#include <log.h>
#include <utility.h>

namespace filetree
{

using namespace MOBase;
using namespace MOShared;

namespace fs = std::filesystem;

constexpr bool AlwaysSortDirectoriesFirst = true;

const QString& directoryFileType()
{
  static QString name;

  if (name.isEmpty()) {
    const DWORD flags = SHGFI_TYPENAME;
    SHFILEINFOW sfi = {};

    // "." for the current directory, which should always exist
    const auto r = SHGetFileInfoW(L".", 0, &sfi, sizeof(sfi), flags);

    if (!r) {
      const auto e = GetLastError();

      log::error(
        "SHGetFileInfoW failed for folder file type, {}",
        formatSystemMessage(e));

      name = "File folder";
    } else {
      name = QString::fromWCharArray(sfi.szTypeName);
    }
  }

  return name;
}


Item::Item(
  Model* model, Item* parent,
  std::wstring dataRelativeParentPath, bool isDirectory, std::wstring file) :
    m_model(model), m_parent(parent), m_indexGuess(NoIndexGuess),
    m_virtualParentPath(QString::fromStdWString(dataRelativeParentPath)),
    m_wsFile(file),
    m_wsLcFile(ToLowerCopy(file)),
    m_key(m_wsLcFile),
    m_file(QString::fromStdWString(file)),
    m_isDirectory(isDirectory),
    m_originID(-1),
    m_flags(NoFlags),
    m_loaded(false),
    m_expanded(false),
    m_sortingStale(true)
{
}

Item::Ptr Item::createFile(
  Model* model, Item* parent,
  std::wstring dataRelativeParentPath, std::wstring file)
{
  return std::unique_ptr<Item>(new Item(
    model, parent, std::move(dataRelativeParentPath), false, std::move(file)));
}

Item::Ptr Item::createDirectory(
  Model* model, Item* parent,
  std::wstring dataRelativeParentPath, std::wstring file)
{
  return std::unique_ptr<Item>(new Item(
    model, parent, std::move(dataRelativeParentPath), true, std::move(file)));
}

void Item::setOrigin(
  int originID, const std::wstring& realPath, Flags flags,
  const std::wstring& mod)
{
  m_originID = originID;
  m_wsRealPath = realPath;
  m_realPath = QString::fromStdWString(realPath);
  m_flags = flags;
  m_mod = QString::fromStdWString(mod);

  m_fileSize.reset();
  m_lastModified.reset();
  m_fileType.reset();
  m_compressedFileSize.reset();
}

void Item::insert(Item::Ptr child, std::size_t at)
{
  if (at > m_children.size()) {
    log::error(
      "{}: can't insert child {} at {}, out of range",
      debugName(), child->debugName(), at);

    return;
  }

  child->m_indexGuess = at;
  m_children.insert(m_children.begin() + at, std::move(child));
}

void Item::remove(std::size_t i)
{
  if (i >= m_children.size()) {
    log::error("{}: can't remove child at {}", debugName(), i);
    return;
  }

  m_children.erase(m_children.begin() + i);
}

void Item::remove(std::size_t from, std::size_t n)
{
  if ((from + n) > m_children.size()) {
    log::error("{}: can't remove children from {} n={}", debugName(), from, n);
    return;
  }

  auto begin = m_children.begin() + from;
  auto end = begin + n;

  m_children.erase(begin, end);
}


template <class T>
int threeWayCompare(T&& a, T&& b)
{
  if (a < b) {
    return -1;
  }

  if (a > b) {
    return 1;
  }

  return 0;
}

class Item::Sorter
{
public:
  static int compare(int column, const Item* a, const Item* b)
  {
    switch (column)
    {
      case Model::FileName:
        return naturalCompare(a->m_file, b->m_file);

      case Model::ModName:
        return naturalCompare(a->m_mod, b->m_mod);

      case Model::FileType:
        return naturalCompare(
          a->fileType().value_or(QString()),
          b->fileType().value_or(QString()));

      case Model::FileSize:
        return threeWayCompare(
          a->fileSize().value_or(0),
          b->fileSize().value_or(0));

      case Model::LastModified:
        return threeWayCompare(
          a->lastModified().value_or(QDateTime()),
          b->lastModified().value_or(QDateTime()));

      default:
        return 0;
    }
  }
};

void Item::sort()
{
  if (!m_children.empty()) {
    m_model->sortItem(*this, true);
  }
}

void Item::sort(int column, Qt::SortOrder order, bool force)
{
  if (!force && !m_expanded) {
    m_sortingStale = true;
    return;
  }

  if (m_sortingStale) {
    //log::debug("sorting is stale for {}, sorting now", debugName());
    m_sortingStale = false;
  }

  std::sort(m_children.begin(), m_children.end(), [&](auto&& a, auto&& b) {
    int r = 0;

    if (a->isDirectory() && !b->isDirectory()) {
      if constexpr (AlwaysSortDirectoriesFirst) {
        return true;
      } else {
        r = -1;
      }
    } else if (!a->isDirectory() && b->isDirectory()) {
      if constexpr (AlwaysSortDirectoriesFirst) {
        return false;
      } else {
        r = 1;
      }
    } else {
      r = Item::Sorter::compare(column, a.get(), b.get());
    }

    if (order == Qt::AscendingOrder) {
      return (r < 0);
    } else {
      return (r > 0);
    }
  });

  for (auto& child : m_children) {
    child->sort(column, order, force);
  }
}

QString Item::virtualPath() const
{
  QString s = "Data\\";

  if (!m_virtualParentPath.isEmpty()) {
    s += m_virtualParentPath + "\\";
  }

  s += m_file;

  return s;
}

QString Item::dataRelativeFilePath() const
{
  auto path = dataRelativeParentPath();
  if (!path.isEmpty()) {
    path += "\\";
  }

  return path += m_file;
}

QFont Item::font() const
{
  QFont f;

  if (isFromArchive()) {
    f.setItalic(true);
  } else if (isHidden()) {
    f.setStrikeOut(true);
  }

  return f;
}

std::optional<uint64_t> Item::fileSize() const
{
  if (m_fileSize.empty() && !m_isDirectory) {
    std::error_code ec;
    const auto size = fs::file_size(fs::path(m_wsRealPath), ec);

    if (ec) {
      log::error("can't get file size for '{}', {}", m_realPath, ec.message());
      m_fileSize.fail();
    } else {
      m_fileSize.set(size);
    }
  }

  return m_fileSize.value;
}

std::optional<QDateTime> Item::lastModified() const
{
  if (m_lastModified.empty()) {
    if (m_realPath.isEmpty()) {
      // this is a virtual directory
      m_lastModified.set({});
    } else if (isFromArchive()) {
      // can't get last modified date for files in archives
      m_lastModified.set({});
    } else {
      // looks like a regular file on the filesystem
      const QFileInfo fi(m_realPath);
      const auto d = fi.lastModified();

      if (!d.isValid()) {
        log::error("can't get last modified date for '{}'", m_realPath);
        m_lastModified.fail();
      } else {
        m_lastModified.set(d);
      }
    }
  }

  return m_lastModified.value;
}

std::optional<QString> Item::fileType() const
{
  if (m_fileType.empty()) {
    getFileType();
  }

  return m_fileType.value;
}

void Item::getFileType() const
{
  if (isDirectory()) {
    m_fileType.set(directoryFileType());
    return;
  }

  DWORD flags = SHGFI_TYPENAME;

  if (isFromArchive()) {
    // files from archives are not on the filesystem; this flag forces
    // SHGetFileInfoW() to only work with the filename
    flags |= SHGFI_USEFILEATTRIBUTES;
  }

  SHFILEINFOW sfi = {};
  const auto r = SHGetFileInfoW(
    m_wsRealPath.c_str(), 0, &sfi, sizeof(sfi), flags);

  if (!r) {
    const auto e = GetLastError();

    log::error(
      "SHGetFileInfoW failed for '{}', {}",
      m_realPath, formatSystemMessage(e));

    m_fileType.fail();
  } else {
    m_fileType.set(QString::fromWCharArray(sfi.szTypeName));
  }
}

QFileIconProvider::IconType Item::icon() const
{
  if (m_isDirectory) {
    return QFileIconProvider::Folder;
  } else {
    return QFileIconProvider::File;
  }
}

bool Item::isHidden() const
{
  return m_file.endsWith(ModInfo::s_HiddenExt, Qt::CaseInsensitive);
}

void Item::unload()
{
  if (!m_loaded) {
    return;
  }

  clear();
}

bool Item::areChildrenVisible() const
{
  if (m_expanded) {
    if (m_parent) {
      return m_parent->areChildrenVisible();
    } else {
      return true;
    }
  }

  return false;
}

QString Item::debugName() const
{
  return QString("%1(ld=%2,cs=%3)")
    .arg(virtualPath())
    .arg(m_loaded)
    .arg(m_children.size());
}

} // namespace
