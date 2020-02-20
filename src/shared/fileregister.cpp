#include "fileregister.h"
#include "fileentry.h"
#include "directoryentry.h"
#include "originconnection.h"
#include "filesorigin.h"
#include <log.h>

namespace MOShared
{

using namespace MOBase;

FileRegister::FileRegister(std::shared_ptr<OriginConnection> originConnection)
  : m_OriginConnection(originConnection), m_NextIndex(0)
{
}

bool FileRegister::indexValid(FileIndex index) const
{
  std::scoped_lock lock(m_Mutex);

  if (index < m_Files.size()) {
    return (m_Files[index].get() != nullptr);
  }

  return false;
}

FileEntryPtr FileRegister::createFile(
  std::wstring name, DirectoryEntry *parent, DirectoryStats& stats)
{
  const auto index = generateIndex();
  auto p = FileEntryPtr(new FileEntry(index, std::move(name), parent));

  {
    std::scoped_lock lock(m_Mutex);

    if (index >= m_Files.size()) {
      m_Files.resize(index + 1);
    }

    m_Files[index] = p;
  }

  return p;
}

FileIndex FileRegister::generateIndex()
{
  return m_NextIndex++;
}

FileEntryPtr FileRegister::getFile(FileIndex index) const
{
  std::scoped_lock lock(m_Mutex);

  if (index < m_Files.size()) {
    return m_Files[index];
  } else {
    return {};
  }
}

bool FileRegister::removeFile(FileIndex index)
{
  std::scoped_lock lock(m_Mutex);

  if (index >= m_Files.size()) {
    log::error(
      "FileRegister::removeFile(): index {} out of range, size is {}",
      index, m_Files.size());

    return false;
  }

  FileEntryPtr file;

  // remove from list
  m_Files[index].swap(file);

  if (!file) {
    log::error("FileRegister::removeFile(): index {} is empty", index);
    return false;
  }

  // unregister from origin
  const OriginID originID = file->getOrigin();
  if (auto* o=m_OriginConnection->findByID(originID)) {
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

void FileRegister::removeOriginMulti(
  std::set<FileIndex> indices, OriginID originID)
{
  std::scoped_lock lock(m_Mutex);

  for (auto&& index : indices) {
    if (index >= m_Files.size()) {
      log::error(
        "FileRegister::removeOriginMulti(): index {} out of range, size is {}",
        index, m_Files.size());

      continue;
    }

    FileEntryPtr file = m_Files[index];

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
      m_Files[index] = {};
    }
  }
}

void FileRegister::sortOrigins()
{
  std::scoped_lock lock(m_Mutex);

  for (auto&& p : m_Files) {
    if (p) {
      p->sortOrigins();
    }
  }
}

} // namespace
