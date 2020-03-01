/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "directoryentry.h"
#include "directoryrefresher.h"
#include "originconnection.h"
#include "filesorigin.h"
#include "fileentry.h"
#include "envfs.h"
#include "util.h"
#include "windows_error.h"
#include <log.h>
#include <utility.h>

namespace MOShared
{

using namespace MOBase;

// calls f(c, last) on each component, where `c` is the name and `last` is
// true only for the last component; if f() returns false, stops the iteration
//
template <class F>
void forEachPathComponent(std::wstring_view path, F&& f)
{
  if (path.empty()) {
    return;
  }

  std::size_t start = 0;

  for (;;) {
    const std::size_t sep = path.find_first_of(L"\\/", start);

    if (sep == std::string::npos) {
      // last component
      f(path.substr(start), true);
      break;
    }

    if (!f(path.substr(start, sep - start), false)) {
      break;
    }

    start = sep + 1;
  }
}


DirectoryEntry::DirectoryEntry(
  std::wstring name, DirectoryEntry* parent, OriginID originID,
  std::shared_ptr<FileRegister> fr)
    : m_register(fr), m_name(std::move(name)), m_parent(parent)
{
  m_origins.insert(originID);
}

std::unique_ptr<DirectoryEntry> DirectoryEntry::createRoot(
  std::shared_ptr<FileRegister> fr)
{
  return std::unique_ptr<DirectoryEntry>(
    new DirectoryEntry(L"data", nullptr, 0, std::move(fr)));
}

std::vector<FileEntryPtr> DirectoryEntry::getFiles() const
{
  std::vector<FileEntryPtr> result;

  if (auto fr=getFileRegister()) {
    for (const auto& [name, index] : m_files) {
      result.push_back(fr->getFile(index));
    }
  }

  return result;
}

OriginID DirectoryEntry::anyOrigin() const
{
  auto fr = getFileRegister();
  if (!fr) {
    return InvalidOriginID;
  }

  // look for any file that's not from an archive
  for (const auto& [name, index] : m_files) {
    if (auto file=fr->getFile(index)) {
      if (!file->isFromArchive()) {
        return file->getOrigin();
      }
    }
  }

  // recurse into subdirectories
  for (const auto& dir : m_dirs) {
    const OriginID o = dir->anyOrigin();
    if (o != InvalidOriginID){
      return o;
    }
  }

  // use an origin from this directory
  if (!m_origins.empty()) {
    return *(m_origins.begin());
  }

  // this directory has no origins
  return InvalidOriginID;
}

const DirectoryEntry* DirectoryEntry::findSubDirectory(
  std::wstring_view name) const
{
  return findSubDirectory(FileKeyView(ToLowerCopy(name)));
}

const DirectoryEntry* DirectoryEntry::findSubDirectory(FileKeyView key) const
{
  auto itor = m_dirsLookup.find(key);
  if (itor == m_dirsLookup.end()) {
    return nullptr;
  }

  return itor->second;
}

const DirectoryEntry* DirectoryEntry::findSubDirectoryRecursive(
  std::wstring_view path, bool alreadyLowerCase) const
{
  if (alreadyLowerCase) {
    return findSubDirectoryRecursiveImpl(path);
  } else {
    return findSubDirectoryRecursiveImpl(ToLowerCopy(path));
  }
}

const DirectoryEntry* DirectoryEntry::findSubDirectoryRecursiveImpl(
  std::wstring_view path) const
{
  const DirectoryEntry* cwd = this;

  forEachPathComponent(path, [&](std::wstring_view name, bool) {
    cwd = cwd->findSubDirectory(FileKeyView(name));
    return (cwd != nullptr);
  });

  return cwd;
}

FileEntryPtr DirectoryEntry::findFile(std::wstring_view name) const
{
  return findFile(FileKeyView(ToLowerCopy(name)));
}

FileEntryPtr DirectoryEntry::findFile(FileKeyView key) const
{
  auto itor = m_filesLookup.find(key);
  if (itor == m_filesLookup.end()) {
    return {};
  }

  auto fr = getFileRegister();
  if (!fr) {
    return {};
  }

  return fr->getFile(itor->second);
}

FileEntryPtr DirectoryEntry::findFileRecursive(
  std::wstring_view path, bool alreadyLowerCase) const
{
  if (alreadyLowerCase) {
    return findFileRecursiveImpl(path);
  } else {
    return findFileRecursiveImpl(ToLowerCopy(path));
  }
}

FileEntryPtr DirectoryEntry::findFileRecursiveImpl(std::wstring_view path) const
{
  FileEntryPtr file;
  const DirectoryEntry* cwd = this;

  forEachPathComponent(path, [&](std::wstring_view name, bool last) {
    if (last) {
      // last component, this must be a file; an empty name means the path
      // ended with a separator and so cannot represent a valid file
      if (!name.empty()) {
        file = cwd->findFile(FileKeyView(name));
      }

      return true;
    } else {
      // this component is a subdirectory
      cwd = cwd->findSubDirectory(FileKeyView(name));
      return (cwd != nullptr);
    }
  });

  return file;
}

void DirectoryEntry::addFromOrigin(
  FilesOrigin& origin, env::DirectoryWalker& walker)
{
  addFiles(walker, origin, origin.getPath());
}

void DirectoryEntry::addFromOrigin(FilesOrigin& origin)
{
  env::DirectoryWalker walker;
  addFromOrigin(origin, walker);
}

void DirectoryEntry::addFromBSA(
  FilesOrigin& origin, const fs::path& archive, int order)
{
  const auto archiveName = archive.filename().native();

  BSA::Archive bsa;
  BSA::EErrorCode res = bsa.read(
    ToString(archive.native(), false).c_str(), false);

  if ((res != BSA::ERROR_NONE) && (res != BSA::ERROR_INVALIDHASHES)) {
    log::error("invalid bsa '{}', error {}", archive, res);
    return;
  }

  std::error_code ec;
  const auto lwt = std::filesystem::last_write_time(archive, ec);
  FILETIME ft = {};

  if (ec) {
    log::warn(
      "failed to get last modified date for '{}', {}",
      archive, ec.message());
  } else {
    ft = ToFILETIME(lwt);
  }

  addFiles(origin, bsa.getRoot(), ft, {archiveName, order});
}

void DirectoryEntry::cleanupIrrelevant()
{
  // names must be lowercase
  static const std::wstring files[] = {L"meta.ini", L"readme.txt"};
  static const std::wstring dirs[] = {L"fomod"};

  auto fr = getFileRegister();
  if (!fr) {
    return;
  }

  for (auto&& f : files) {
    auto itor = m_filesLookup.find(FileKeyView(f));
    if (itor != m_filesLookup.end()) {
      // this will eventually call removeFile() on this directory
      fr->removeFile(itor->second);
    }
  }

  for (auto&& d : dirs) {
    auto itor = m_dirsLookup.find(FileKeyView(d));
    if (itor != m_dirsLookup.end()) {
      removeDirectory(itor);
    }
  }
}

void DirectoryEntry::propagateOriginInternal(OriginID origin)
{
  DirectoryEntry* d = this;

  while (d) {
    {
      std::scoped_lock lock(d->m_originsMutex);
      d->m_origins.insert(origin);
    }

    d = d->m_parent;
  }
}

std::wstring DirectoryEntry::debugName() const
{
  return m_name;
}

void DirectoryEntry::removeFileInternal(std::wstring_view name)
{
  const auto lcname = ToLowerCopy(name);

  {
    auto itor = m_files.find(lcname);
    if (itor == m_files.end()) {
      log::error("DirectoryEntry::removeFile(): '{}' not in list", name);
    } else {
      m_files.erase(itor);
    }
  }

  {
    auto itor = m_filesLookup.find(FileKeyView(lcname));
    if (itor == m_filesLookup.end()) {
      log::error("DirectoryEntry::removeFile(): '{}' not in lookup", name);
    } else {
      m_filesLookup.erase(itor);
    }
  }
}

// given to the DirectoryWalker, passed to every callback
//
struct DirectoryEntry::Context
{
  // origin the files are being added from
  FilesOrigin& origin;

  // register, avoids locking the weak_ptr for every file
  std::shared_ptr<FileRegister> fr;

  // stack of directories, pushed in onDirectoryStart(), popped in
  // onDirectoryEnd()
  std::stack<DirectoryEntry*> current;
};

void DirectoryEntry::addFiles(
  env::DirectoryWalker& walker, FilesOrigin& origin, const std::wstring& path)
{
  Context cx = {origin, m_register.lock()};
  cx.current.push(this);

  walker.forEachEntry(path, &cx,
    [](void* pcx, std::wstring_view path)
    {
      onDirectoryStart((Context*)pcx, path);
    },

    [](void* pcx, std::wstring_view path)
    {
      onDirectoryEnd((Context*)pcx, path);
    },

    [](void* pcx, std::wstring_view path, FILETIME ft)
    {
      onFile((Context*)pcx, path, ft);
    }
  );

  sortSubDirectories();
}

void DirectoryEntry::onDirectoryStart(Context* cx, std::wstring_view path)
{
  // creates the directory and pushes it on the stack

  auto* sd = cx->current.top()->getOrCreateSubDirectory(
    path, cx->origin.getID());

  cx->current.push(sd);
}

void DirectoryEntry::onDirectoryEnd(Context* cx, std::wstring_view path)
{
  // sorts the directory and pops it

  auto* current = cx->current.top();
  current->sortSubDirectories();
  cx->current.pop();
}

void DirectoryEntry::onFile(Context* cx, std::wstring_view path, FILETIME ft)
{
  // adds the file to the current directory
  cx->fr->addFile(*cx->current.top(), path, cx->origin, ft, {});
}

void DirectoryEntry::sortSubDirectories()
{
  std::scoped_lock lock(m_dirsMutex);

  std::sort(m_dirs.begin(), m_dirs.end(), [](auto&& lhs, auto&& rhs) {
    return (_wcsicmp(lhs->getName().c_str(), rhs->getName().c_str()) < 0);
  });
}

void DirectoryEntry::addFiles(
  FilesOrigin& origin, const BSA::Folder::Ptr& archiveFolder,
  FILETIME archiveFileTime, const ArchiveInfo& archive)
{
  auto fr = m_register.lock();
  if (!fr) {
    return;
  }

  // add files
  const auto fileCount = archiveFolder->getNumFiles();

  for (unsigned int i=0; i<fileCount; ++i) {
    const BSA::File::Ptr file = archiveFolder->getFile(i);

    auto f = fr->addFile(
      *this, ToWString(file->getName(), true),
      origin, archiveFileTime, archive);

    if (f) {
      if (file->getUncompressedFileSize() > 0) {
        f->setFileSize(file->getUncompressedFileSize());
        f->setCompressedFileSize(file->getFileSize());
      } else {
        f->setFileSize(file->getFileSize());
      }
    }
  }

  // recurse into subdirectories
  const auto dirCount = archiveFolder->getNumSubFolders();
  for (unsigned int i=0; i<dirCount; ++i) {
    const BSA::Folder::Ptr folder = archiveFolder->getSubFolder(i);

    DirectoryEntry* folderEntry = getOrCreateSubDirectories(
      ToWString(folder->getName(), true), origin.getID());

    folderEntry->addFiles(origin, folder, archiveFileTime, archive);
  }
}

DirectoryEntry* DirectoryEntry::getOrCreateSubDirectory(
  std::wstring_view name, OriginID originID)
{
  std::wstring nameLc = ToLowerCopy(name);

  std::scoped_lock lock(m_dirsMutex);

  auto itor = m_dirsLookup.find(FileKeyView(nameLc));
  if (itor != m_dirsLookup.end()) {
    // already exists
    return itor->second;
  }

  return addSubDirectory(
    std::wstring(name.begin(), name.end()),
    std::move(nameLc), originID);
}

DirectoryEntry* DirectoryEntry::addSubDirectory(
  std::wstring name, std::wstring nameLc, OriginID originID)
{
  std::unique_ptr<DirectoryEntry> entry(new DirectoryEntry(
    std::move(name), this, originID, m_register.lock()));

  // name is moved from this point

  auto* p = entry.get();

  m_dirsLookup.emplace(std::move(nameLc), entry.get());
  m_dirs.push_back(std::move(entry));
  // nameLc is moved from this point

  return p;
}

FileEntryPtr DirectoryEntry::addFileInternal(std::wstring_view name)
{
  auto fr = getFileRegister();
  if (!fr) {
    return {};
  }

  FileKey key(ToLowerCopy(name));

  std::unique_lock lock(m_filesMutex);
  auto itor = m_filesLookup.find(key);

  if (itor != m_filesLookup.end()) {
    // file exists
    lock.unlock();
    return fr->getFile(itor->second);
  } else {
    // file not found, create it
    auto fe = fr->createFile(
      std::wstring(name.begin(), name.end()), this);

    m_filesLookup.emplace(key.value, fe->getIndex());
    m_files.emplace(std::move(key.value), fe->getIndex());
    // key has been moved from this point

    return fe;
  }
}

DirectoryEntry* DirectoryEntry::getOrCreateSubDirectories(
  std::wstring_view path, OriginID originID)
{
  DirectoryEntry* cwd = this;

  forEachPathComponent(path, [&](std::wstring_view name, bool) {
    cwd = cwd->getOrCreateSubDirectory(name, originID);
    return true;
  });

  return cwd;
}

void DirectoryEntry::removeDirectory(SubDirectoriesLookup::iterator itor)
{
  // remove files from subdirectories
  itor->second->removeSelfRecursive();

  auto itor2 = std::find_if(m_dirs.begin(), m_dirs.end(), [&](auto&& d) {
    return (d.get() == itor->second);
  });

  if (itor2 == m_dirs.end()) {
    log::error("entry {} not in sub directories map", itor->second->getName());
  } else {
    m_dirs.erase(itor2);
  }

  // careful: from this point, the directory entry has been deleted

  m_dirsLookup.erase(itor);
}

void DirectoryEntry::removeSelfRecursive()
{
  auto fr = getFileRegister();
  if (!fr) {
    return;
  }

  // careful: FileRegister::removeFile() eventually calls removeFile() on this
  // directory
  while (!m_files.empty()) {
    fr->removeFile(m_files.begin()->second);
  }

  m_filesLookup.clear();

  for (auto& entry : m_dirs) {
    entry->removeSelfRecursive();
  }

  m_dirs.clear();
  m_dirsLookup.clear();
}


struct DumpFailed : public std::runtime_error
{
  using runtime_error::runtime_error;
};

void DirectoryEntry::dump(const std::wstring& file) const
{
  try
  {
    std::FILE* f = nullptr;
    auto e = _wfopen_s(&f, file.c_str(), L"wb");

    if (e != 0 || !f) {
      throw DumpFailed(fmt::format(
        "failed to open, {} ({})", std::strerror(e), e));
    }

    Guard g([&]{ std::fclose(f); });

    dump(f, L"Data");
  }
  catch(DumpFailed& e)
  {
    log::error(
      "failed to write list to '{}': {}",
      QString::fromStdWString(file).toStdString(), e.what());
  }
}

void DirectoryEntry::dump(std::FILE* f, const std::wstring& parentPath) const
{
  auto fr = getFileRegister();
  if (!fr) {
    return;
  }

  auto oc = getOriginConnection();
  if (!oc) {
    return;
  }

  {
    std::scoped_lock lock(m_filesMutex);

    for (const auto& [name, index]: m_files) {
      const auto file = fr->getFile(index);

      if (!file) {
        log::debug(
          "DirectoryEntry::dump(): file index {} in directory '{}' not "
          "found in register", index, getName());
        continue;
      }

      if (file->isFromArchive()) {
        // TODO: don't list files from archives. maybe make this an option?
        continue;
      }

      const auto* o = oc->findByID(file->getOrigin());

      if (!o) {
        log::error(
          "while dumping directory entry '{}', "
          "cannot found origin '{}' for file '{}'",
          debugName(), file->getOrigin(), file->getName());

        continue;
      }

      const auto path = parentPath + L"\\" + file->getName();
      const auto line = path + L"\t(" + o->getName() + L")\r\n";

      const auto lineu8 = MOShared::ToString(line, true);

      if (std::fwrite(lineu8.data(), lineu8.size(), 1, f) != 1) {
        const auto e = errno;
        throw DumpFailed(fmt::format(
          "failed to write, {} ({})", std::strerror(e), e));
      }
    }
  }

  {
    std::scoped_lock lock(m_dirsMutex);
    for (auto&& d : m_dirs) {
      const auto path = parentPath + L"\\" + d->m_name;
      d->dump(f, path);
    }
  }
}

} // namespace MOShared
