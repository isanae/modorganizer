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

template <class F>
void elapsedImpl(std::chrono::nanoseconds& out, F&& f)
{
  if constexpr (DirectoryStats::EnableInstrumentation) {
    const auto start = std::chrono::high_resolution_clock::now();
    f();
    const auto end = std::chrono::high_resolution_clock::now();
    out += (end - start);
  } else {
    f();
  }
}

// elapsed() is not optimized out when EnableInstrumentation is false even
// though it's equivalent that this macro
#define elapsed(OUT, F) (F)();
//#define elapsed(OUT, F) elapsedImpl(OUT, F);

static bool DirCompareByName(
  const std::unique_ptr<DirectoryEntry>& lhs,
  const std::unique_ptr<DirectoryEntry>& rhs)
{
  return _wcsicmp(lhs->getName().c_str(), rhs->getName().c_str()) < 0;
}

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
      f(path, true);
      break;
    }

    if (!f(path.substr(0, sep), false)) {
      break;
    }

    start = sep + 1;
  }
}


DirectoryEntry::DirectoryEntry(
  std::wstring name, DirectoryEntry* parent, OriginID originID,
  std::shared_ptr<FileRegister> fileRegister,
  std::shared_ptr<OriginConnection> originConnection) :
    m_FileRegister(fileRegister), m_OriginConnection(originConnection),
    m_Name(std::move(name)), m_Parent(parent)
{
  m_Origins.insert(originID);
}

std::unique_ptr<DirectoryEntry> DirectoryEntry::createRoot()
{
  auto oc = OriginConnection::create();
  auto fr = std::make_shared<FileRegister>(oc);

  return std::unique_ptr<DirectoryEntry>(
    new DirectoryEntry(L"data", nullptr, 0, fr, oc));
}

std::vector<FileEntryPtr> DirectoryEntry::getFiles() const
{
  std::vector<FileEntryPtr> result;

  for (const auto& [name, index] : m_Files) {
    result.push_back(m_FileRegister->getFile(index));
  }

  return result;
}

bool DirectoryEntry::originExists(std::wstring_view name) const
{
  return m_OriginConnection->exists(name);
}

FilesOrigin& DirectoryEntry::getOriginByID(OriginID id) const
{
  return m_OriginConnection->getByID(id);
}

FilesOrigin& DirectoryEntry::getOriginByName(std::wstring_view name) const
{
  return m_OriginConnection->getByName(name);
}

const FilesOrigin* DirectoryEntry::findOriginByID(OriginID id) const
{
  return m_OriginConnection->findByID(id);
}

OriginID DirectoryEntry::anyOrigin() const
{
  // look for any file that's not from an archive
  for (const auto& [name, index] : m_Files) {
    if (auto file=m_FileRegister->getFile(index)) {
      if (!file->isFromArchive()) {
        return file->getOrigin();
      }
    }
  }

  // recurse into subdirectories
  for (const auto& dir : m_SubDirectories) {
    const OriginID o = dir->anyOrigin();
    if (o != InvalidOriginID){
      return o;
    }
  }

  // use an origin from this directory
  if (!m_Origins.empty()) {
    return *(m_Origins.begin());
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
  auto itor = m_SubDirectoriesLookup.find(key);
  if (itor == m_SubDirectoriesLookup.end()) {
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
  auto itor = m_FilesLookup.find(key);
  if (itor == m_FilesLookup.end()) {
    return {};
  }

  return m_FileRegister->getFile(itor->second);
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

FilesOrigin& DirectoryEntry::getOrCreateOrigin(const OriginInfo& originInfo)
{
  return m_OriginConnection->getOrCreateOrigin(
    originInfo.name, originInfo.path, originInfo.priority, m_FileRegister)
      .first;
}


void DirectoryEntry::addFromOrigin(
  const OriginInfo& originInfo,
  env::DirectoryWalker& walker, DirectoryStats& stats)
{
  FilesOrigin &origin = getOrCreateOrigin(originInfo);

  if (!originInfo.path.empty()) {
    addFiles(walker, origin, originInfo.path, stats);
  }
}

void DirectoryEntry::addFromOrigin(
  const OriginInfo& originInfo, DirectoryStats& stats)
{
  env::DirectoryWalker walker;
  addFromOrigin(originInfo, walker, stats);
}

void DirectoryEntry::addFromBSA(
  const OriginInfo& originInfo, const fs::path& archive,
  int order, DirectoryStats& stats)
{
  FilesOrigin& origin = getOrCreateOrigin(originInfo);

  const auto archiveName = archive.filename().native();

  if (containsArchive(archiveName)) {
    return;
  }

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

  addFiles(origin, bsa.getRoot(), ft, archiveName, order, stats);
}

void DirectoryEntry::propagateOrigin(OriginID origin)
{
  {
    std::scoped_lock lock(m_OriginsMutex);
    m_Origins.insert(origin);
  }

  if (m_Parent != nullptr) {
    m_Parent->propagateOrigin(origin);
  }
}

bool DirectoryEntry::containsArchive(std::wstring archiveName)
{
  for (auto iter = m_Files.begin(); iter != m_Files.end(); ++iter) {
    FileEntryPtr entry = m_FileRegister->getFile(iter->second);
    if (entry->isFromArchive(archiveName)) {
      return true;
    }
  }

  return false;
}

void DirectoryEntry::removeFile(FileIndex index)
{
  removeFileFromList(index);
}

bool DirectoryEntry::removeFile(const std::wstring &filePath, OriginID* origin)
{
  size_t pos = filePath.find_first_of(L"\\/");

  if (pos == std::string::npos) {
    return this->remove(filePath, origin);
  }

  std::wstring dirName = filePath.substr(0, pos);
  std::wstring rest = filePath.substr(pos + 1);

  DirectoryStats dummy;
  DirectoryEntry* entry = findSubDirectoryRecursive(dirName);

  if (entry != nullptr) {
    return entry->removeFile(rest, origin);
  } else {
    return false;
  }
}

void DirectoryEntry::removeDir(const std::wstring &path)
{
  size_t pos = path.find_first_of(L"\\/");

  if (pos == std::string::npos) {
    for (auto iter = m_SubDirectories.begin(); iter != m_SubDirectories.end(); ++iter) {
      auto& entry = *iter;

      if (CaseInsensitiveEqual(entry->getName(), path)) {
        entry->removeDirRecursive();
        removeDirectoryFromList(iter);
        break;
      }
    }
  } else {
    std::wstring dirName = path.substr(0, pos);
    std::wstring rest = path.substr(pos + 1);

    DirectoryStats dummy;
    DirectoryEntry* entry = findSubDirectoryRecursive(dirName);

    if (entry != nullptr) {
      entry->removeDir(rest);
    }
  }
}

bool DirectoryEntry::remove(const std::wstring &fileName, OriginID* origin)
{
  const auto lcFileName = ToLowerCopy(fileName);

  auto iter = m_Files.find(lcFileName);
  bool b = false;

  if (iter != m_Files.end()) {
    if (origin != nullptr) {
      FileEntryPtr entry = m_FileRegister->getFile(iter->second);
      if (entry.get() != nullptr) {
        bool ignore;
        *origin = entry->getOrigin(ignore);
      }
    }

    b = m_FileRegister->removeFile(iter->second);
  }

  return b;
}

void DirectoryEntry::removeFiles(const std::set<FileIndex> &indices)
{
  removeFilesFromList(indices);
}

FileEntryPtr DirectoryEntry::insert(
  std::wstring_view fileName, FilesOrigin &origin, FILETIME fileTime,
  std::wstring_view archive, int order, DirectoryStats& stats)
{
  std::wstring fileNameLower = ToLowerCopy(fileName);
  FileEntryPtr fe;

  FileKey key(std::move(fileNameLower));
  // fileNameLower has moved from this point

  {
    std::unique_lock lock(m_FilesMutex);

    FilesLookup::iterator itor;

    elapsed(stats.filesLookupTimes, [&]{
      itor = m_FilesLookup.find(key);
    });

    if (itor != m_FilesLookup.end()) {
      lock.unlock();
      fe = m_FileRegister->getFile(itor->second);
    } else {
      fe = m_FileRegister->createFile(
        std::wstring(fileName.begin(), fileName.end()), this, stats);

      elapsed(stats.addFileTimes, [&] {
        addFileToList(std::move(key.value), fe->getIndex());
      });
    }
  }

  elapsed(stats.addOriginToFileTimes, [&]{
    fe->addOrigin(origin.getID(), fileTime, archive, order);
  });

  elapsed(stats.addFileToOriginTimes, [&]{
    origin.addFile(fe->getIndex());
  });

  return fe;
}

struct DirectoryEntry::Context
{
  FilesOrigin& origin;
  DirectoryStats& stats;
  std::stack<DirectoryEntry*> current;
};

void DirectoryEntry::addFiles(
  env::DirectoryWalker& walker, FilesOrigin &origin,
  const std::wstring& path, DirectoryStats& stats)
{
  Context cx = {origin, stats};
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

  {
    std::scoped_lock lock(m_SubDirMutex);

    std::sort(
      m_SubDirectories.begin(),
      m_SubDirectories.end(),
      &DirCompareByName);
  }
}

void DirectoryEntry::onDirectoryStart(Context* cx, std::wstring_view path)
{
  elapsed(cx->stats.dirTimes, [&] {
    auto* sd = cx->current.top()->createSubDirectory(
      path, cx->origin.getID(), cx->stats);

    cx->current.push(sd);
  });
}

void DirectoryEntry::onDirectoryEnd(Context* cx, std::wstring_view path)
{
  elapsed(cx->stats.dirTimes, [&] {
    auto* current = cx->current.top();

    {
      std::scoped_lock lock(current->m_SubDirMutex);

      std::sort(
        current->m_SubDirectories.begin(),
        current->m_SubDirectories.end(),
        &DirCompareByName);
    }

    cx->current.pop();
  });
}

void DirectoryEntry::onFile(Context* cx, std::wstring_view path, FILETIME ft)
{
  elapsed(cx->stats.fileTimes, [&]{
    cx->current.top()->insert(path, cx->origin, ft, L"", -1, cx->stats);
  });
}

void DirectoryEntry::addFiles(
  FilesOrigin& origin, const BSA::Folder::Ptr archiveFolder, FILETIME fileTime,
  const std::wstring& archiveName, int order, DirectoryStats& stats)
{
  // add files
  const auto fileCount = archiveFolder->getNumFiles();
  for (unsigned int i=0; i<fileCount; ++i) {
    const BSA::File::Ptr file = archiveFolder->getFile(i);

    auto f = insert(
      ToWString(file->getName(), true), origin, fileTime,
      archiveName, order, stats);

    if (f) {
      if (file->getUncompressedFileSize() > 0) {
        f->setFileSize(file->getFileSize(), file->getUncompressedFileSize());
      } else {
        f->setFileSize(file->getFileSize(), FileEntry::NoFileSize);
      }
    }
  }

  // recurse into subdirectories
  const auto dirCount = archiveFolder->getNumSubFolders();
  for (unsigned int i=0; i<dirCount; ++i) {
    const BSA::Folder::Ptr folder = archiveFolder->getSubFolder(i);

    DirectoryEntry* folderEntry = createSubDirectories(
      ToWString(folder->getName(), true), origin.getID(), stats);

    folderEntry->addFiles(origin, folder, fileTime, archiveName, order, stats);
  }
}

DirectoryEntry* DirectoryEntry::createSubDirectory(
  std::wstring_view name, OriginID originID, DirectoryStats& stats)
{
  std::wstring nameLc = ToLowerCopy(name);

  std::scoped_lock lock(m_SubDirMutex);

  SubDirectoriesLookup::iterator itor;
  elapsed(stats.subdirLookupTimes, [&] {
    itor = m_SubDirectoriesLookup.find(FileKeyView(nameLc));
  });

  if (itor != m_SubDirectoriesLookup.end()) {
    // already exists
    return itor->second;
  }

  std::unique_ptr<DirectoryEntry> entry(new DirectoryEntry(
    std::wstring(name.begin(), name.end()), this, originID,
    m_FileRegister, m_OriginConnection));

  auto* p = entry.get();

  elapsed(stats.addDirectoryTimes, [&] {
    addDirectoryToList(std::move(entry), std::move(nameLc));
    // nameLc is moved from this point
  });

  return p;
}

DirectoryEntry* DirectoryEntry::createSubDirectories(
  std::wstring_view path, OriginID originID, DirectoryStats& stats)
{
  DirectoryEntry* cwd = this;

  forEachPathComponent(path, [&](std::wstring_view name, bool) {
    cwd = cwd->createSubDirectory(name, originID, stats);
    return true;
  });

  return cwd;
}

void DirectoryEntry::removeDirRecursive()
{
  while (!m_Files.empty()) {
    m_FileRegister->removeFile(m_Files.begin()->second);
  }

  m_FilesLookup.clear();

  for (auto& entry : m_SubDirectories) {
    entry->removeDirRecursive();
  }

  m_SubDirectories.clear();
  m_SubDirectoriesLookup.clear();
}

void DirectoryEntry::addDirectoryToList(
  std::unique_ptr<DirectoryEntry> e, std::wstring nameLc)
{
  m_SubDirectoriesLookup.emplace(std::move(nameLc), e.get());
  m_SubDirectories.push_back(std::move(e));
}

void DirectoryEntry::removeDirectoryFromList(SubDirectories::iterator itor)
{
  const auto& entry = *itor;

  {
    auto itor2 = std::find_if(
      m_SubDirectoriesLookup.begin(), m_SubDirectoriesLookup.end(),
      [&](auto&& d) { return (d.second == entry.get()); });

    if (itor2 == m_SubDirectoriesLookup.end()) {
      log::error("entry {} not in sub directories map", entry->getName());
    } else {
      m_SubDirectoriesLookup.erase(itor2);
    }
  }

  m_SubDirectories.erase(itor);
}

void DirectoryEntry::removeFileFromList(FileIndex index)
{
  auto removeFrom = [&](auto& list) {
    auto iter = std::find_if(
      list.begin(), list.end(),
      [&index](auto&& pair) { return (pair.second == index); }
    );

    if (iter == list.end()) {
      auto f = m_FileRegister->getFile(index);

      if (f) {
        log::error(
          "can't remove file '{}', not in directory entry '{}'",
          f->getName(), getName());
      } else {
        log::error(
          "can't remove file with index {}, not in directory entry '{}' and "
          "not in register",
          index, getName());
      }
    } else {
      list.erase(iter);
    }
  };

  removeFrom(m_FilesLookup);
  removeFrom(m_Files);
}

void DirectoryEntry::removeFilesFromList(const std::set<FileIndex>& indices)
{
  for (auto iter = m_Files.begin(); iter != m_Files.end();) {
    if (indices.find(iter->second) != indices.end()) {
      iter = m_Files.erase(iter);
    } else {
      ++iter;
    }
  }

  for (auto iter = m_FilesLookup.begin(); iter != m_FilesLookup.end();) {
    if (indices.find(iter->second) != indices.end()) {
      iter = m_FilesLookup.erase(iter);
    } else {
      ++iter;
    }
  }
}

void DirectoryEntry::addFileToList(std::wstring fileNameLower, FileIndex index)
{
  m_FilesLookup.emplace(fileNameLower, index);
  m_Files.emplace(std::move(fileNameLower), index);
  // fileNameLower has been moved from this point
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
  {
    std::scoped_lock lock(m_FilesMutex);

    for (auto&& index : m_Files) {
      const auto file = m_FileRegister->getFile(index.second);
      if (!file) {
        continue;
      }

      if (file->isFromArchive()) {
        // TODO: don't list files from archives. maybe make this an option?
        continue;
      }

      const auto& o = m_OriginConnection->getByID(file->getOrigin());
      const auto path = parentPath + L"\\" + file->getName();
      const auto line = path + L"\t(" + o.getName() + L")\r\n";

      const auto lineu8 = MOShared::ToString(line, true);

      if (std::fwrite(lineu8.data(), lineu8.size(), 1, f) != 1) {
        const auto e = errno;
        throw DumpFailed(fmt::format(
          "failed to write, {} ({})", std::strerror(e), e));
      }
    }
  }

  {
    std::scoped_lock lock(m_SubDirMutex);
    for (auto&& d : m_SubDirectories) {
      const auto path = parentPath + L"\\" + d->m_Name;
      d->dump(f, path);
    }
  }
}

} // namespace MOShared
