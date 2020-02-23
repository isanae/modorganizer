#include "fileentry.h"
#include "directoryentry.h"
#include "filesorigin.h"

namespace MOShared
{

FileEntry::FileEntry(FileIndex index, std::wstring name, DirectoryEntry *parent) :
  m_Index(index), m_Name(std::move(name)), m_Origin(-1), m_Archive(L"", -1),
  m_Parent(parent)
{
}

FileEntryPtr FileEntry::create(
  FileIndex index, std::wstring name, DirectoryEntry *parent)
{
  return FileEntryPtr(new FileEntry(index, std::move(name), parent));
}

void FileEntry::addOrigin(
  OriginID origin, FILETIME fileTime, const ArchiveInfo& archive)
{
  std::scoped_lock lock(m_OriginsMutex);

  if (m_Parent != nullptr) {
    m_Parent->propagateOrigin(origin);
  }

  if (m_Origin == -1) {
    // If this file has no previous origin, this mod is now the origin with no
    // alternatives
    m_Origin = origin;
    m_FileTime = fileTime;
    m_Archive = archive;
  }
  else if (
    (m_Parent != nullptr) && (
    (m_Parent->getOriginByID(origin).getPriority() > m_Parent->getOriginByID(m_Origin).getPriority()) ||
      (archive.name.size() == 0 && m_Archive.name.size() > 0 ))
    ) {
    // If this mod has a higher priority than the origin mod OR
    // this mod has a loose file and the origin mod has an archived file,
    // this mod is now the origin and the previous origin is the first alternative

    auto itor = std::find_if(
      m_Alternatives.begin(), m_Alternatives.end(),
      [&](auto&& i) { return i.originID == m_Origin; });

    if (itor == m_Alternatives.end()) {
      m_Alternatives.push_back({m_Origin, m_Archive});
    }

    m_Origin = origin;
    m_FileTime = fileTime;
    m_Archive = archive;
  }
  else {
    // This mod is just an alternative
    bool found = false;

    if (m_Origin == origin) {
      // already an origin
      return;
    }

    for (auto iter = m_Alternatives.begin(); iter != m_Alternatives.end(); ++iter) {
      if (iter->originID == origin) {
        // already an origin
        return;
      }

      if ((m_Parent != nullptr) &&
        (m_Parent->getOriginByID(iter->originID).getPriority() < m_Parent->getOriginByID(origin).getPriority())) {
        m_Alternatives.insert(iter, {origin, archive});
        found = true;
        break;
      }
    }

    if (!found) {
      m_Alternatives.push_back({origin, archive});
    }
  }
}

bool FileEntry::removeOrigin(OriginID origin)
{
  std::scoped_lock lock(m_OriginsMutex);

  if (m_Origin == origin) {
    if (!m_Alternatives.empty()) {
      // find alternative with the highest priority
      auto currentIter = m_Alternatives.begin();
      for (auto iter = m_Alternatives.begin(); iter != m_Alternatives.end(); ++iter) {
        if (iter->originID != origin) {
          //Both files are not from archives.
          if (!iter->archive.name.size() && !currentIter->archive.name.size()) {
            if ((m_Parent->getOriginByID(iter->originID).getPriority() > m_Parent->getOriginByID(currentIter->originID).getPriority())) {
              currentIter = iter;
            }
          }
          else {
            //Both files are from archives
            if (iter->archive.name.size() && currentIter->archive.name.size()) {
              if (iter->archive.order > currentIter->archive.order) {
                currentIter = iter;
              }
            }
            else {
              // Only one of the two is an archive, so we change currentIter
              // only if he is the archive one.
              if (currentIter->archive.name.size()) {
                currentIter = iter;
              }
            }
          }
        }
      }

      OriginID currentID = currentIter->originID;
      m_Archive = currentIter->archive;
      m_Alternatives.erase(currentIter);

      m_Origin = currentID;
    } else {
      m_Origin = -1;
      m_Archive = {};
      return true;
    }
  } else {
    auto newEnd = std::remove_if(
      m_Alternatives.begin(), m_Alternatives.end(),
      [&](auto &i) { return i.originID == origin; });

    if (newEnd != m_Alternatives.end()) {
      m_Alternatives.erase(newEnd, m_Alternatives.end());
    }
  }
  return false;
}

void FileEntry::sortOrigins()
{
  std::scoped_lock lock(m_OriginsMutex);

  m_Alternatives.push_back({m_Origin, m_Archive});

  std::sort(m_Alternatives.begin(), m_Alternatives.end(), [&](auto&& LHS, auto&& RHS) {
    if (!LHS.archive.name.size() && !RHS.archive.name.size()) {
      int l = m_Parent->getOriginByID(LHS.originID).getPriority();
      if (l < 0) {
        l = INT_MAX;
      }

      int r = m_Parent->getOriginByID(RHS.originID).getPriority();
      if (r < 0) {
        r = INT_MAX;
      }

      return l < r;
    }

    if (LHS.archive.name.size() && RHS.archive.name.size()) {
      int l = LHS.archive.order; if (l < 0) l = INT_MAX;
      int r = RHS.archive.order; if (r < 0) r = INT_MAX;

      return l < r;
    }

    if (RHS.archive.name.size()) {
      return false;
    }

    return true;
  });

  if (!m_Alternatives.empty()) {
    m_Origin = m_Alternatives.back().originID;
    m_Archive = m_Alternatives.back().archive;
    m_Alternatives.pop_back();
  }
}

bool FileEntry::existsInArchive(std::wstring_view archiveName) const
{
  if (m_Archive.name == archiveName) {
    return true;
  }

  for (auto alternative : m_Alternatives) {
    if (alternative.archive.name == archiveName) {
      return true;
    }
  }

  return false;
}

bool FileEntry::isFromArchive() const
{
  std::scoped_lock lock(m_OriginsMutex);
  return !m_Archive.name.empty();
}

std::wstring FileEntry::getFullPath(OriginID originID) const
{
  if (originID == InvalidOriginID) {
    std::scoped_lock lock(m_OriginsMutex);
    originID = m_Origin;
  }

  // base directory for origin
  const auto* o = m_Parent->findOriginByID(originID);
  if (!o) {
    return {};
  }

  return o->getPath() + L"\\" + getRelativePath();
}

std::wstring FileEntry::getRelativePath() const
{
  std::wstring path;
  const DirectoryEntry* e = m_Parent;

  while (e) {
    if (!e->getParent()) {
      // the next directory would be top-most, which is the Data directory;
      // skip it
      break;
    }

    path = e->getName() + L"\\" + path;
    e = e->getParent();
  }

  path += m_Name;

  return path;
}

} // namespace
