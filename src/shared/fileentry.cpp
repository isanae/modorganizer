#include "fileentry.h"
#include "directoryentry.h"
#include "filesorigin.h"
#include <log.h>

namespace MOShared
{

using namespace MOBase;

FileEntry::FileEntry(FileIndex index, std::wstring name, DirectoryEntry* p)
  : m_index(index), m_name(std::move(name)), m_parent(p)
{
}

FileEntryPtr FileEntry::create(
  FileIndex index, std::wstring name, DirectoryEntry* parent)
{
  return FileEntryPtr(new FileEntry(index, std::move(name), parent));
}

std::wstring FileEntry::getFullPath(OriginID originID) const
{
  if (originID == InvalidOriginID) {
    // use primary when no specific origin is given
    std::scoped_lock lock(m_originsMutex);
    originID = m_origin.originID;
  }

  const auto* o = m_parent->findOriginByID(originID);
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
  const DirectoryEntry* e = m_parent;

  while (e) {
    if (e->isTopLevel()) {
      // don't add the top-level name to the path (such as Data/)
      break;
    }

    path = e->getName() + L"\\" + path;
    e = e->getParent();
  }

  path += m_name;

  return path;
}

bool FileEntry::existsInArchive(std::wstring_view archiveName) const
{
  // check primary origin
  if (m_origin.archive.name == archiveName) {
    return true;
  }

  // check alternatives
  for (const auto& o : m_alternatives) {
    if (o.archive.name == archiveName) {
      return true;
    }
  }

  return false;
}

bool FileEntry::isFromArchive() const
{
  std::scoped_lock lock(m_originsMutex);
  return !m_origin.archive.name.empty();
}

void FileEntry::addOrigin(const OriginInfo& newOrigin, FILETIME fileTime)
{
  std::scoped_lock lock(m_originsMutex);

  if (m_origin.originID == newOrigin.originID) {
    // already an origin
    log::warn(
      "cannot add origin {} to file {}, already the primary origin",
      newOrigin.debugName(), debugName());

    return;
  }

  if (m_parent) {
    // also add the origin to the parent directories
    m_parent->propagateOrigin(newOrigin.originID);
  }

  if (shouldReplacePrimaryOrigin(newOrigin)) {
    setPrimaryOrigin(newOrigin, fileTime);
  } else {
    addAlternativeOrigin(newOrigin);
  }
}

bool FileEntry::removeOrigin(OriginID removeOriginID)
{
  std::scoped_lock lock(m_originsMutex);

  if (m_origin.originID == removeOriginID) {
    // the origin that was removed is currently the primary one...

    if (m_alternatives.empty()) {
      // ...and there are no other origins for this file
      //
      // this can happen when:
      //   1) a file is about to be removed from the registry because all its
      //      origins are gone, or
      //
      //   2) when switching the origin of this file from Data to a pseudo-mod
      //      in DirectoryStructure::addAssociatedFiles()
      m_origin = {};
      return true;
    } else {
      // ...but there are still other available origins, grab the one with the
      // highest priority and remove it from the list of alternatives
      auto itor = std::prev(m_alternatives.end());
      m_origin = std::move(*itor);
      m_alternatives.erase(itor);
      assertAlternativesSorted();
    }
  } else {
    // the origin is an alternative, remove it from the list

    auto itor = findAlternativeByID(removeOriginID);

    if (itor == m_alternatives.end()) {
      log::warn(
        "for file {}, cannot remove origin {}, not in alternative list",
        debugName(), removeOriginID);
    } else {
      m_alternatives.erase(itor);
      assertAlternativesSorted();
    }
  }

  return false;
}

void FileEntry::sortOrigins()
{
  std::vector<OriginInfo> v;

  {
    std::scoped_lock lock(m_originsMutex);
    v = m_alternatives;
    v.push_back(m_origin);
  }

  std::sort(
    v.begin(), v.end(),
    [&](auto&& a, auto&& b) { return (comparePriorities(a, b) < 0); });

  {
    std::scoped_lock lock(m_originsMutex);
    m_origin = std::move(*std::prev(v.end()));
    m_alternatives.assign(v.begin(), v.end() - 1);
  }

  assertAlternativesSorted();
}

std::wstring FileEntry::debugName() const
{
  return fmt::format(L"{}:{}", m_name, m_index);
}

bool FileEntry::shouldReplacePrimaryOrigin(const OriginInfo& newOrigin) const
{
  if (m_origin.originID == InvalidOriginID) {
    // this file doesn't have a valid origin right now
    return true;
  }

  if (m_parent == nullptr) {
    // shouldn't happen
    return true;
  }

  // if the new origin has a higher priority, take it
  if (comparePriorities(m_origin, newOrigin) < 0) {
    return true;
  }

  // the origin is an alternative
  return false;
}

void FileEntry::setPrimaryOrigin(const OriginInfo& newOrigin, FILETIME ft)
{
  // the given origin must replace the current one, if any; if there _is_ a
  // current origin, move it to the alternatives
  if (m_origin.originID != InvalidOriginID) {
    // make sure the origin isn't already in the alternatives, which shouldn't
    // happen
    auto itor = findAlternativeByID(m_origin.originID);

    if (itor == m_alternatives.end()) {
      m_alternatives.push_back(m_origin);
      assertAlternativesSorted();
    } else {
      log::warn(
        "for file {}, while moving the current origin {} to alternatives so "
        "{} can become primary, the id already exists as {}",
        debugName(), m_origin.debugName(),
        newOrigin.debugName(), itor->debugName());
    }
  }

  m_origin = newOrigin;
  m_fileTime = ft;
}

void FileEntry::addAlternativeOrigin(const OriginInfo& newOrigin)
{
  bool found = false;

  for (auto itor=m_alternatives.begin(); itor!=m_alternatives.end(); ++itor) {
    if (itor->originID == newOrigin.originID) {
      // already an origin
      log::warn(
        "for file {}, cannot add {} as an alternative because it's already "
        "in the list as {}",
        debugName(), newOrigin.debugName(), itor->debugName());

      return;
    }

    if (comparePriorities(*itor, newOrigin) < 0) {
      m_alternatives.insert(itor, newOrigin);
      assertAlternativesSorted();
      return;
    }
  }

  m_alternatives.push_back(newOrigin);
  assertAlternativesSorted();
}

int FileEntry::comparePriorities(const OriginInfo& a, const OriginInfo& b) const
{
  if (!m_parent) {
    // shouldn't happen
    return 0;
  }

  auto* aOrigin = m_parent->findOriginByID(a.originID);
  auto* bOrigin = m_parent->findOriginByID(b.originID);

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
    m_alternatives.begin(), m_alternatives.end(), [&](auto&& i) {
      return (i.originID == id);
    });
}

void FileEntry::assertAlternativesSorted() const
{
  auto v = m_alternatives;
  v.push_back(m_origin);

  const bool sorted = std::is_sorted(
    v.begin(), v.end(),
    [&](auto&& a, auto&& b) { return (comparePriorities(a, b) < 0); });

  if (!sorted) {
    DebugBreak();
  }
}

} // namespace
