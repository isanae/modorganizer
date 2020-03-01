#include "fileregister.h"
#include "fileentry.h"
#include "directoryentry.h"
#include "originconnection.h"
#include "filesorigin.h"
#include "util.h"
#include <log.h>

using namespace MOBase;

FileRegister::FileRegister()
  : m_fileCount(0)
{
  // note that m_originConnection is set in create() because it needs the
  // shared_ptr
}

std::shared_ptr<FileRegister> FileRegister::create()
{
  std::shared_ptr<FileRegister> p(new FileRegister);
  p->m_originConnection = OriginConnection::create(p);
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

FileEntryPtr FileRegister::createFileInternal(
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

FileEntryPtr FileRegister::addFile(
  DirectoryEntry& parent, std::wstring_view name, FilesOrigin& origin,
  FILETIME fileTime, const ArchiveInfo& archive)
{
  FileKey key(ToLowerCopy(name));

  auto fe = parent.addFileInternal(name);

  // add the origin to the file
  fe->addOriginInternal({origin.getID(), archive}, fileTime);

  // add the file to the origin
  origin.addFileInternal(fe->getIndex());

  return fe;
}

void FileRegister::removeFile(FileIndex index)
{
  std::scoped_lock lock(m_mutex);

  if (index >= m_files.size()) {
    log::error(
      "FileRegister::removeFile(): index {} out of range, size is {}",
      index, m_files.size());

    return;
  }

  FileEntryPtr file;

  // remove from list
  m_files[index].swap(file);

  if (!file) {
    log::error("FileRegister::removeFile(): index {} is empty", index);
    return;
  }

  --m_fileCount;

  // unregister from primary origin
  if (auto* o=m_originConnection->findByID(file->getOrigin())) {
    o->removeFileInternal(file->getIndex());
  }

  // unregister from other origins
  for (const auto& [altID, unused] : file->getAlternatives()) {
    if (auto* o=m_originConnection->findByID(altID)) {
      o->removeFileInternal(file->getIndex());
    }
  }

  // unregister from directory
  if (auto* dir=file->getParent()) {
    dir->removeFileInternal(file->getName());
  }

  // remove all of the file's origins
  file->removeAllOriginsInternal();
}

void FileRegister::changeFileOrigin(
  DirectoryEntry& root, std::wstring_view relativePath,
  FilesOrigin& from, FilesOrigin& to)
{
  const auto file = root.findFileRecursive(relativePath);

  if (!file) {
    log::error(
      "cannot change origin for file '{}' from {} to {}, "
      "file was not found in the directories",
      relativePath, from.debugName(), to.debugName());

    return;
  }


  fs::path newPath(to.getPath() / relativePath);

  std::error_code ec;
  const auto lastModified = fs::last_write_time(newPath, ec);
  FILETIME ft = {};

  if (ec) {
    log::warn(
      "while changing file origin for {} from {} to {}, "
      "could not get last modified time from real path {}: {}",
      relativePath, from.debugName(), to.debugName(), newPath, ec.message());
  } else {
    ft = ToFILETIME(lastModified);
  }


  // removing file from origin
  from.removeFileInternal(file->getIndex());

  // remove origin from file
  file->removeOriginInternal(from.getID());


  // add file to origin
  to.addFileInternal(file->getIndex());

  // add origin to file
  file->addOriginInternal({to.getID(), {}}, ft);
}

void FileRegister::disableOrigin(FilesOrigin& o)
{
  for (auto& index : o.getFileIndices()) {
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

    // removeOriginInternal() returns true when the last origin was removed
    // from the file; in that case, the file has to be removed from the
    // directory
    if (file->removeOriginInternal(o.getID())) {
      // remove from directory
      if (auto* dir=file->getParent()) {
        dir->removeFileInternal(file->getName());
      }

      // remove from list
      m_files[index] = {};
      --m_fileCount;
    }
  }

  o.clearFilesInternal();
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
  return m_originConnection;
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
