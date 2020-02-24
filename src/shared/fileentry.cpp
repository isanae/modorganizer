#include "fileentry.h"
#include "directoryentry.h"
#include "filesorigin.h"
#include <log.h>

namespace MOShared
{

using namespace MOBase;

FileEntry::FileEntry(FileIndex index, std::wstring name, DirectoryEntry* p)
  : m_Index(index), m_Name(std::move(name)), m_Parent(p)
{
}

FileEntryPtr FileEntry::create(
  FileIndex index, std::wstring name, DirectoryEntry* parent)
{
  return FileEntryPtr(new FileEntry(index, std::move(name), parent));
}

void FileEntry::addOrigin(const OriginInfo& newOrigin, FILETIME fileTime)
{
  std::scoped_lock lock(m_OriginsMutex);

  if (m_Origin.originID == newOrigin.originID) {
    // already an origin
    log::warn(
      "cannot add origin {} to file {}, already the primary origin",
      newOrigin.debugName(), debugName());

    return;
  }

  if (m_Parent) {
    // also add the origin to the parent directories
    m_Parent->propagateOrigin(newOrigin.originID);
  }

  if (shouldReplacePrimaryOrigin(newOrigin)) {
    setPrimaryOrigin(newOrigin, fileTime);
  } else {
    addAlternativeOrigin(newOrigin);
  }
}

int FileEntry::comparePriorities(const OriginInfo& a, const OriginInfo& b) const
{
  if (!m_Parent) {
    // shouldn't happen
    return 0;
  }

  auto* aOrigin = m_Parent->findOriginByID(a.originID);
  auto* bOrigin = m_Parent->findOriginByID(b.originID);

  if (aOrigin == bOrigin) {
    // same origin
    return true;
  }

  // shouldn't happen, but an origin that's not found will be sorted lower
  if (aOrigin && !bOrigin) {
    return -1;
  } else if (!aOrigin && bOrigin) {
    return 1;
  }

  // getting origin priorities (basically the order in the mod list)
  const auto aPriority = aOrigin->getPriority();
  const auto bPriority = bOrigin->getPriority();

  if (aPriority < bPriority) {
    return -1;
  } else if (aPriority > bPriority) {
    return 1;
  }

  // if both origins have the same priority, the one that is not from an
  // archive is ranked higher

  const bool aFromArchive = !a.archive.name.empty();
  const bool bFromArchive = !b.archive.name.empty();

  if (!aFromArchive && bFromArchive) {
    return -1;
  } else if (aFromArchive && !bFromArchive) {
    return 1;
  }

  // the two origins effectively have the same priority

  return 0;
}

std::vector<OriginInfo>::const_iterator FileEntry::findAlternativeByID(OriginID id) const
{
  return std::find_if(
    m_Alternatives.begin(), m_Alternatives.end(), [&](auto&& i) {
      return (i.originID == id);
    });
}

bool FileEntry::shouldReplacePrimaryOrigin(const OriginInfo& newOrigin) const
{
  if (m_Origin.originID == InvalidOriginID) {
    // this file doesn't have a valid origin right now
    return true;
  }

  if (m_Parent == nullptr) {
    // shouldn't happen
    return true;
  }

  // if the new origin has a higher priority, take it
  if (comparePriorities(m_Origin, newOrigin) < 0) {
    return true;
  }

  // the origin is an alternative
  return false;
}

void FileEntry::setPrimaryOrigin(const OriginInfo& newOrigin, FILETIME ft)
{
  // the given origin must replace the current one, if any; if there _is_ a
  // current origin, move it to the alternatives
  if (m_Origin.originID != InvalidOriginID) {
    // make sure the origin isn't already in the alternatives, which shouldn't
    // happen
    auto itor = findAlternativeByID(m_Origin.originID);

    if (itor == m_Alternatives.end()) {
      m_Alternatives.push_back(m_Origin);
      assertAlternativesSorted();
    } else {
      log::warn(
        "for file {}, while moving the current origin {} to alternatives so "
        "{} can become primary, the id already exists as {}",
        debugName(), m_Origin.debugName(),
        newOrigin.debugName(), itor->debugName());
    }
  }

  m_Origin = newOrigin;
  m_FileTime = ft;
}

void FileEntry::addAlternativeOrigin(const OriginInfo& newOrigin)
{
  bool found = false;

  for (auto itor=m_Alternatives.begin(); itor!=m_Alternatives.end(); ++itor) {
    if (itor->originID == newOrigin.originID) {
      // already an origin
      log::warn(
        "for file {}, cannot add {} as an alternative because it's already "
        "in the list as {}",
        debugName(), newOrigin.debugName(), itor->debugName());

      return;
    }

    if (comparePriorities(*itor, newOrigin) < 0) {
      m_Alternatives.insert(itor, newOrigin);
      assertAlternativesSorted();
      return;
    }
  }

  m_Alternatives.push_back(newOrigin);
  assertAlternativesSorted();
}

bool FileEntry::removeOrigin(OriginID removeOriginID)
{
  std::scoped_lock lock(m_OriginsMutex);

  if (m_Origin.originID == removeOriginID) {
    // the origin that was removed is currently the primary one...

    if (m_Alternatives.empty()) {
      // ...and there are no other origins for this file
      //
      // this can happen when:
      //   1) a file is about to be removed from the registry because all its
      //      origins are gone, or
      //
      //   2) when switching the origin of this file from Data to a pseudo-mod
      //      in DirectoryStructure::addAssociatedFiles()
      m_Origin = {};
      return true;
    } else {
      // ...but there are still other available origins, grab the one with the
      // highest priority and remove it from the list of alternatives
      auto itor = std::prev(m_Alternatives.end());
      m_Origin = std::move(*itor);
      m_Alternatives.erase(itor);
      assertAlternativesSorted();
    }
  } else {
    // the origin is an alternative, remove it from the list

    auto itor = findAlternativeByID(removeOriginID);

    if (itor == m_Alternatives.end()) {
      log::warn(
        "for file {}, cannot remove origin {}, not in alternative list",
        debugName(), removeOriginID);
    } else {
      m_Alternatives.erase(itor);
      assertAlternativesSorted();
    }
  }

  return false;
}

void FileEntry::sortOrigins()
{
  std::vector<OriginInfo> v;

  {
    std::scoped_lock lock(m_OriginsMutex);
    v = m_Alternatives;
    v.push_back(m_Origin);
  }

  std::sort(
    v.begin(), v.end(),
    [&](auto&& a, auto&& b) { return (comparePriorities(a, b) < 0); });

  {
    std::scoped_lock lock(m_OriginsMutex);
    m_Origin = std::move(*std::prev(v.end()));
    m_Alternatives.assign(v.begin(), v.end() - 1);
  }

  assertAlternativesSorted();
}

void FileEntry::assertAlternativesSorted() const
{
  auto v = m_Alternatives;
  v.push_back(m_Origin);

  const bool sorted = std::is_sorted(
    v.begin(), v.end(),
    [&](auto&& a, auto&& b) { return (comparePriorities(a, b) < 0); });

  if (!sorted) {
    DebugBreak();
  }
}

bool FileEntry::existsInArchive(std::wstring_view archiveName) const
{
  // check primary origin
  if (m_Origin.archive.name == archiveName) {
    return true;
  }

  // check alternatives
  for (const auto& o : m_Alternatives) {
    if (o.archive.name == archiveName) {
      return true;
    }
  }

  return false;
}

bool FileEntry::isFromArchive() const
{
  std::scoped_lock lock(m_OriginsMutex);
  return !m_Origin.archive.name.empty();
}

std::wstring FileEntry::getFullPath(OriginID originID) const
{
  if (originID == InvalidOriginID) {
    // use primary when no specific origin is given
    std::scoped_lock lock(m_OriginsMutex);
    originID = m_Origin.originID;
  }

  const auto* o = m_Parent->findOriginByID(originID);
  if (!o) {
    log::error(
      "for file {}, can't get full path for origin {}, origin not found",
      debugName(), originID);

    return {};
  }

  return o->getPath() + L"\\" + getRelativePath();
}

std::wstring FileEntry::getRelativePath() const
{
  std::wstring path;
  const DirectoryEntry* e = m_Parent;

  while (e) {
    if (e->isTopLevel()) {
      // don't add the top-level name to the path (such as Data/)
      break;
    }

    path = e->getName() + L"\\" + path;
    e = e->getParent();
  }

  path += m_Name;

  return path;
}

std::wstring FileEntry::debugName() const
{
  return fmt::format(L"{}:{}", m_Name, m_Index);
}

} // namespace
