#include "fileregister.h"
#include "fileentry.h"
#include "directoryentry.h"
#include "originconnection.h"
#include "filesorigin.h"
#include "util.h"
#include <log.h>

namespace MOShared
{

using namespace MOBase;

FileRegister::FileRegister()
  : m_fileCount(0)
{
  // note that m_OriginConnection is set in create() because it needs the
  // shared_ptr
}

std::shared_ptr<FileRegister> FileRegister::create()
{
  std::shared_ptr<FileRegister> p(new FileRegister);
  p->m_OriginConnection = OriginConnection::create(p);
  return p;
}

bool FileRegister::fileExists(FileIndex index) const
{
  std::scoped_lock lock(m_mutex);

  if (index >= m_files.size()) {
    return false;
  }

  return (m_files[index].get() != nullptr);
}

FileEntryPtr FileRegister::getFile(FileIndex index) const
{
  std::scoped_lock lock(m_mutex);

  if (index >= m_files.size()) {
    return {};
  }

  return m_files[index];
}

FileEntryPtr FileRegister::createFile(
  std::wstring name, DirectoryEntry* parent)
{
  FileMap::iterator itor;
  FileIndex index;

  {
    std::scoped_lock lock(m_mutex);

    m_files.push_back({});

    itor = std::prev(m_files.end());
    index = static_cast<FileIndex>(m_files.size() - 1);
    ++m_fileCount;
  }

  auto p = FileEntry::create(index, std::move(name), parent);
  *itor = p;

  return p;
}

bool FileRegister::removeFile(FileIndex index)
{
  std::scoped_lock lock(m_mutex);

  if (index >= m_files.size()) {
    log::error(
      "FileRegister::removeFile(): index {} out of range, size is {}",
      index, m_files.size());

    return false;
  }

  FileEntryPtr file;

  // remove from list
  m_files[index].swap(file);

  if (!file) {
    log::error("FileRegister::removeFile(): index {} is empty", index);
    return false;
  }

  --m_fileCount;

  // unregister from primary origin
  if (auto* o=m_OriginConnection->findByID(file->getOrigin())) {
    o->removeFile(file->getIndex());
  }

  // unregister from other origins
  for (const auto& [altID, unused] : file->getAlternatives()) {
    if (auto* o=m_OriginConnection->findByID(altID)) {
      o->removeFile(file->getIndex());
    }
  }

  // unregister from directory
  if (auto* dir=file->getParent()) {
    dir->removeFile(file->getName());
  }

  return true;
}

void FileRegister::removeOrigin(
  std::set<FileIndex> indices, OriginID originID)
{
  std::scoped_lock lock(m_mutex);

  for (auto&& index : indices) {
    if (index >= m_files.size()) {
      log::error(
        "FileRegister::removeOriginMulti(): index {} out of range, size is {}",
        index, m_files.size());

      continue;
    }

    FileEntryPtr file = m_files[index];

    if (!file) {
      log::error("FileRegister::removeOriginMulti(): index {} is empty", index);
      continue;
    }

    // removeOrigin() returns true when the last origin was removed from the
    // file; in that case, the file has to be removed from the directory
    if (file->removeOrigin(originID)) {
      // remove from directory
      if (auto* dir=file->getParent()) {
        dir->removeFile(file->getName());
      }

      // remove from list
      m_files[index] = {};
      --m_fileCount;
    }
  }
}

void FileRegister::sortOrigins()
{
  std::scoped_lock lock(m_mutex);

  for (auto&& p : m_files) {
    if (p) {
      p->sortOrigins();
    }
  }
}

std::shared_ptr<OriginConnection> FileRegister::getOriginConnection() const
{
  return m_OriginConnection;
}


std::wstring ArchiveInfo::debugName() const
{
  return fmt::format(L"{}:{}", name, order);
}

std::ostream& operator<<(std::ostream& out, const ArchiveInfo& a)
{
  out << ToString(a.debugName(), true);
  return out;
}


std::wstring OriginInfo::debugName() const
{
  return fmt::format(
    L"{}:{}",
    originID,
    archive.name.empty() ? L"loose" : archive.name);
}

std::ostream& operator<<(std::ostream& out, const OriginInfo& a)
{
  out << ToString(a.debugName(), true);
  return out;
}

} // namespace
