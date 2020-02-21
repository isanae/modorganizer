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

#ifndef MO_REGISTER_DIRECTORYENTRY_INCLUDED
#define MO_REGISTER_DIRECTORYENTRY_INCLUDED

#include "fileregister.h"
#include <bsatk.h>

namespace env
{
  class DirectoryWalker;
}


namespace MOShared
{

// a directory has files, subdirectories and a list of origins having the
// directory
//
// threads-safety: this class is thread-safe when adding stuff to it, mostly
// so it can be used with DirectoryStructure; however, most of the functions
// that are used later (getFiles(), etc.) do not use the mutexes and therefore
// are not thread-safe
//
// this is for performance, but also because of laziness
//
class DirectoryEntry
{
public:
  using SubDirectories = std::vector<std::unique_ptr<DirectoryEntry>>;

  struct OriginInfo
  {
    std::wstring_view name;
    fs::path path;
    int priority;
  };


  // creates a root directory
  //
  static std::unique_ptr<DirectoryEntry> createRoot();

  // non-copyable
  DirectoryEntry(const DirectoryEntry&) = delete;
  DirectoryEntry& operator=(const DirectoryEntry&) = delete;


  // whether this is the root directory
  //
  bool isTopLevel() const
  {
    return (m_parent == nullptr);
  }

  // whether this directory is empty of files and subdirectories
  //
  bool isEmpty() const
  {
    return (m_files.empty() && m_dirs.empty());
  }

  // whether this directory has files
  //
  bool hasFiles() const
  {
    return !m_files.empty();
  }

  // this directory's parent, may be null if this is a root directory
  //
  const DirectoryEntry* getParent() const
  {
    return m_parent;
  }

  // directory name
  //
  const std::wstring& getName() const
  {
    return m_name;
  }

  // returns files that are inside this directory; each file inside this
  // directory has to looked up in the register by index and added to a new
  // vector
  //
  // if only file indexes are required, prefer forEachFileIndex()
  //
  std::vector<FileEntryPtr> getFiles() const;

  // returns directories that are inside this directory
  //
  const SubDirectories& getSubDirectories() const
  {
    return m_dirs;
  }

  // returns the associated file register; all directories and files share the
  // same register
  //
  std::shared_ptr<FileRegister> getFileRegister() const
  {
    return m_register;
  }

  // forwards to OriginConnection::exists()
  //
  bool originExists(std::wstring_view name) const;

  // forwards to OriginConnection::getByID()
  //
  FilesOrigin& getOriginByID(OriginID id) const;

  // forwards to OriginConnection::getByName()
  //
  FilesOrigin& getOriginByName(std::wstring_view name) const;

  // forwards to OriginConnection::findByID()
  //
  const FilesOrigin* findOriginByID(OriginID id) const;


  // returns an arbitrary origin that contains this directory or either a file
  // or a subdirectory inside this directory, recursively; returns
  // InvalidOriginID if this directory has no children nor origins itself
  //
  OriginID anyOrigin() const;

  // calls f() for every subdirectory inside this directory; if f() returns
  // false, the iteration stops
  //
  template <class F>
  void forEachDirectory(F&& f) const
  {
    for (auto&& d : m_dirs) {
      if (!f(*d)) {
        break;
      }
    }
  }

  // calls f() for every file inside this directory; if f() returns false, the
  // iteration stops
  //
  // this is less efficient than forEachFileIndex() because each index has to
  // be looked up in the register; if only file indexes are required, prefer
  // forEachFileIndex()
  //
  template <class F>
  void forEachFile(F&& f) const
  {
    for (auto&& p : m_files) {
      if (auto file=m_register->getFile(p.second)) {
        if (!f(*file)) {
          break;
        }
      }
    }
  }

  // calls f() for every file index inside this directory; if f() returns
  // false, the iteration stops; see forEachFile()
  //
  template <class F>
  void forEachFileIndex(F&& f) const
  {
    for (auto&& p : m_files) {
      if (!f(p.second)) {
        break;
      }
    }
  }


  // looks for a directory that's an immediate child of this one and has the
  // given name, may return null
  //
  // `name` will be lowercased before it's looked up; `key` must be constructed
  // from a string that's already lowercase and so can be used as-is
  //
  const DirectoryEntry* findSubDirectory(std::wstring_view name) const;
  const DirectoryEntry* findSubDirectory(FileKeyView key) const;

  DirectoryEntry* findSubDirectory(std::wstring_view name)
  {
    // forward
    return const_cast<DirectoryEntry*>(
      std::as_const(*this).findSubDirectory(name));
  }

  DirectoryEntry* findSubDirectory(FileKeyView key)
  {
    // forward
    return const_cast<DirectoryEntry*>(
      std::as_const(*this).findSubDirectory(key));
  }

  // looks for a directory that's somewhere below this one; returns this for
  // an empty path, may return null if the path is not found
  //
  // `path` is split on '/' and '\' and each component is looked up
  // recursively; `path` will be lowercased before it's looked up
  //
  // `alreadyLowerCase` should be set to `true` if `path` is already lowercase
  // so that no case conversion is performed
  //
  const DirectoryEntry* findSubDirectoryRecursive(
    std::wstring_view path, bool alreadyLowerCase=false) const;

  DirectoryEntry* findSubDirectoryRecursive(
    std::wstring_view path, bool alreadyLowerCase=false)
  {
    // forward
    return const_cast<DirectoryEntry*>(
      std::as_const(*this).findSubDirectoryRecursive(path, alreadyLowerCase));
  }


  // looks for a file that's an immediate child of this directory and has the
  // given name, may return an empty FileEntryPtr
  //
  // `name` will be lowercased before it's looked up; `key` must be constructed
  // from a string that's already lowercase and so can be used as-is
  //
  FileEntryPtr findFile(std::wstring_view name) const;
  FileEntryPtr findFile(FileKeyView key) const;

  // looks for a file that's somewhere below this directory; may return an
  // empty FileEntryPtr if the path is not found
  //
  // `path` is split on '/' and '\', each component except the last one is
  // assumed to be a subdirectory and the last one a file; `path` will be
  // lowercased before it's looked up
  //
  // returns an empty FileEntryPtr if `path` is empty or ends with a path
  // separator
  //
  // `alreadyLowerCase` should be set to `true` if `path` is already lowercase
  // so that no case conversion is performed
  //
  FileEntryPtr findFileRecursive(
    std::wstring_view path, bool alreadyLowerCase=false) const;


  FilesOrigin& getOrCreateOrigin(const OriginInfo& originInfo);

  // adds files to this directory recursively from the specified origin; uses
  // the given DirectoryWalker as an optimization
  //
  void addFromOrigin(
    const OriginInfo& originInfo,
    env::DirectoryWalker& walker, DirectoryStats& stats);

  // convenience; forwards to the above with a new DirectoryWalker
  //
  void addFromOrigin(const OriginInfo& origin, DirectoryStats& stats);

  // parses the given archive and adds all files from it to this directory
  //
  void addFromBSA(
    const OriginInfo& originInfo, const fs::path& archive, int order,
    DirectoryStats& stats);

  // remove files from the directory structure that are known to be
  // irrelevant to the game
  //
  void cleanupIrrelevant();

  // adds the given origin to this directory and its parent recursively
  //
  void propagateOrigin(OriginID origin);

  // removes the given file from this directory
  //
  void removeFile(std::wstring_view name);

  void dump(const std::wstring& file) const;

private:
  using FilesMap = std::map<std::wstring, FileIndex>;

  // note equal_to<> is a transparent comparator to allow find() to work with
  // both FileKey and FileKeyView
  using FilesLookup = std::unordered_map<
    FileKey, FileIndex, std::hash<FileKey>, std::equal_to<>>;

  using SubDirectoriesLookup = std::unordered_map<
    FileKey, DirectoryEntry*, std::hash<FileKey>, std::equal_to<>>;

  // these are shared across all directories
  std::shared_ptr<FileRegister> m_register;
  std::shared_ptr<OriginConnection> m_connection;

  // directory name
  std::wstring m_name;


  // map of files, sorted
  FilesMap m_files;

  // hash map of files for lookups
  FilesLookup m_filesLookup;

  // vector of directories, sorted
  SubDirectories m_dirs;

  // hash map of directories for lookups
  SubDirectoriesLookup m_dirsLookup;

  // parent directory
  DirectoryEntry* m_parent;

  // origins having this directory
  std::set<OriginID> m_origins;


  // protects m_files and m_filesLookup
  mutable std::mutex m_filesMutex;

  // protects m_dirs and m_dirsLookup
  mutable std::mutex m_dirsMutex;

  // protects m_origins
  mutable std::mutex m_originsMutex;


  DirectoryEntry(
    std::wstring name, DirectoryEntry* parent, OriginID originID,
    std::shared_ptr<FileRegister> fileRegister,
    std::shared_ptr<OriginConnection> originConnection);


  // helpers for findSubDirectoryRecursive() and findFileRecursive()
  //
  const DirectoryEntry* findSubDirectoryRecursiveImpl(std::wstring_view path) const;
  FileEntryPtr findFileRecursiveImpl(std::wstring_view path) const;

  // inserts the given file in the list and in the register
  //
  FileEntryPtr insert(
    std::wstring_view fileName, FilesOrigin& origin, FILETIME fileTime,
    std::wstring_view archive, int order, DirectoryStats& stats);

  // walks each file and directory in the given path recursively, calls
  // onDirectoryStart() when a directory is entered, onDirectoryEnd() when
  // a directory is finished and onFile() for every file
  //
  void addFiles(
    env::DirectoryWalker& walker, FilesOrigin& origin,
    const std::wstring& path, DirectoryStats& stats);

  struct Context;
  static void onDirectoryStart(Context* cx, std::wstring_view path);
  static void onDirectoryEnd(Context* cx, std::wstring_view path);
  static void onFile(Context* cx, std::wstring_view path, FILETIME ft);

  // sorts the subdirectories by name, case insensitive
  //
  void sortSubDirectories();

  // adds all the files and folders from the given archive recursively
  //
  void addFiles(
    FilesOrigin& origin, const BSA::Folder::Ptr& archiveFolder,
    FILETIME archiveFileTime, const std::wstring& archiveName,
    int order, DirectoryStats& stats);

  // creates the given subdirectory or returns an existing one
  //
  DirectoryEntry* getOrCreateSubDirectory(
    std::wstring_view name, OriginID originID, DirectoryStats& stats);

  // splits `path` on separators and calls getOrCreateSubDirectory()
  // recursively
  //
  DirectoryEntry* getOrCreateSubDirectories(
    std::wstring_view path, OriginID originID, DirectoryStats& stats);

  // calls removeSelfRecursive() on the given directory, then removes it from
  // both lists in this directory, which deletes the associated DirectoryEntry
  //
  void removeDirectory(SubDirectoriesLookup::iterator itor);

  // removes the files recursively from the register
  //
  void removeSelfRecursive();

  // dumps this directory recursively in the given file, parentPath is used
  // to build the complete relative path
  //
  void dump(std::FILE* f, const std::wstring& parentPath) const;
};

} // namespace MOShared

#endif // MO_REGISTER_DIRECTORYENTRY_INCLUDED
