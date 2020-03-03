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


namespace details
{

// calls f(component, last) for every path component, where `last` is true
// if it's the last component
//
// for example, "a/b" will call f("a", false) and f("b", true)
//
// `component` is never empty, so "a//b" and "/a/b/" are the same as above; an
// empty path does not call f() at all
//
// return false from `f()` to stop processing early
//
template <class F>
void forEachPathComponent(std::wstring_view path, F&& f)
{
  // what complicates things a bit is that firing f() for the last component
  // must pass `true`, which means that when a valid range is found, it must be
  // put on hold until an additional valid range has been found, at which point
  // the first range can be fired safely with `last` being `false`
  //
  // it'd be easy to just allocate a vector and split the path, but this is
  // called pretty often, so avoiding memory allocation is worth it
  //
  // cleanup is done at the end after the whole path has been processed to
  // make sure pending ranges are fired correctly


  // a [begin, end[ range in `path`
  //
  struct Range
  {
    std::size_t begin, end;
    bool empty() const { return (begin == end); }
  };


  // this is set when a range has been found but the path hasn't been
  // completely processed yet, so it's unknown whether it's the last range
  Range pendingRange = {0, 0};

  // start of the current range
  std::size_t start = 0;


  // fires f() with the given range
  auto fire = [&](Range r, bool last) {
    auto sub = path.substr(r.begin, r.end - r.begin);
    return f(sub, last);
  };


  // called every time a separator is found in `path`; fires f() for the
  // range on hold, if any, while making sure empty ranges are ignored
  auto addRange = [&](Range newRange) {
    // the callback can return false to stop the iteration immediately
    bool b = true;

    // newRange is empty when two separators are contiguous or when the first
    // character in the path is a separator; ignore that
    if (!newRange.empty()) {
      // pendingRange is empty for the first range
      if (!pendingRange.empty()) {
        // there is a pendingRange, and newRange has been confirmed as valid;
        // so pendingRange can be safely fired because it is confirmed that it
        // is not the last one
        b = fire(pendingRange, false);
      }

      // the new range is now pending because it's unknown whether it's the
      // last one
      pendingRange = newRange;
    }

    // newRange.end is the separator, so one past it is now the beginning of a
    // potential range
    start = newRange.end + 1;

    // forward what f() returned, if it was called
    return b;
  };


  // for each character in the path
  for (std::size_t i=0; i<path.size(); ++i) {
    const wchar_t& c = path[i];

    // if the character is a separator, create a new range
    if (c == L'/' || c == L'\\') {
      if (!addRange({start, i})) {
        return;
      }
    }
  }


  // a range that extends to the end of the path
  Range lastRange = {start, path.size()};

  // at this point, there are four possible situations:

  if (!pendingRange.empty() && !lastRange.empty())
  {
    // both pendingRange and lastRange are valid ranges; for example, with
    // "a/b", pendingRange is "a" and lastRange is "b"
    //
    // so pendingRange must be fired with `last` being false, and lastRange
    // with `last` being true
    //
    if (!fire(pendingRange, false)) {
      // still have to honour f()'s return value
      return;
    }

    fire(lastRange, true);
  }
  else if (!pendingRange.empty())
  {
    // there is a pendingRange, but lastRange is empty; for example, with
    // "a/", pendingRange is "a", but lastRange is empty
    //
    // so pendingRange must be fired with `last` being true and lastRange is
    // ignored
    fire(pendingRange, true);
  }
  else if (!lastRange.empty())
  {
    // there is a lastRange, but no pendingRange; for example, with "a",
    // pendingRange is empty, but lastRange is "a"
    //
    // so pendingRange is ignored and lastRange is fired with `last` being
    // true
    fire(lastRange, true);
  }
  else
  {
    // both ranges are empty, which happens for an empty path; both are
    // ignored
  }
}

} // namespace details


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


  // creates a root directory
  //
  static std::unique_ptr<DirectoryEntry> createRoot(
    std::shared_ptr<FileRegister> fr);

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
    return m_register.lock();
  }

  // convenience: forwards to getFileRegister()->getOriginConnection()
  //
  std::shared_ptr<OriginConnection> getOriginConnection() const
  {
    if (auto r=getFileRegister()) {
      return r->getOriginConnection();
    }

    return {};
  }


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
    if (auto r=getFileRegister()) {
      for (auto&& p : m_files) {
        if (auto file=r->getFile(p.second)) {
          if (!f(*file)) {
            break;
          }
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


  // adds files to this directory recursively from the specified origin; uses
  // the given DirectoryWalker as an optimization
  //
  void addFromOrigin(FilesOrigin& origin, env::DirectoryWalker& walker);

  // convenience; forwards to the above with a new DirectoryWalker
  //
  void addFromOrigin(FilesOrigin& origin);

  // parses the given archive and adds all files from it to this directory
  //
  void addFromBSA(FilesOrigin& origin, const fs::path& archive, int order);

  // manually adds a subdirectory to this one
  //
  DirectoryEntry* addSubDirectory(
    std::wstring name, std::wstring nameLowercase, OriginID originID);

  // convenience: same as above but has to create a lowercase copy of `name`
  //
  DirectoryEntry* addSubDirectory(std::wstring name, OriginID originID);

  // removes the given subdirectory from this directory
  //
  void removeSubDirectoryInternal(std::wstring_view name);

  // adds the given file this directory
  //
  FileEntryPtr addFileInternal(std::wstring_view name);

  // removes the given file from this directory
  //
  void removeFileInternal(std::wstring_view name);

  // remove files from the directory structure that are known to be
  // irrelevant to the game
  //
  void cleanupIrrelevant();

  // adds the given origin to this directory and its parent recursively
  //
  void propagateOriginInternal(OriginID origin);

  std::wstring debugName() const;

  void dump(const std::wstring& file) const;

private:
  using FilesMap = std::map<std::wstring, FileIndex>;

  // note equal_to<> is a transparent comparator to allow find() to work with
  // both FileKey and FileKeyView
  using FilesLookup = std::unordered_map<
    FileKey, FileIndex, std::hash<FileKey>, std::equal_to<>>;

  using SubDirectoriesLookup = std::unordered_map<
    FileKey, DirectoryEntry*, std::hash<FileKey>, std::equal_to<>>;

  // global register
  std::weak_ptr<FileRegister> m_register;

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
    std::shared_ptr<FileRegister> fr);


  // helpers for findSubDirectoryRecursive() and findFileRecursive()
  //
  const DirectoryEntry* findSubDirectoryRecursiveImpl(
    std::wstring_view path) const;

  FileEntryPtr findFileRecursiveImpl(std::wstring_view path) const;

  // walks each file and directory in the given path recursively, calls
  // onDirectoryStart() when a directory is entered, onDirectoryEnd() when
  // a directory is finished and onFile() for every file
  //
  void addFiles(
    env::DirectoryWalker& walker, FilesOrigin& origin,
    const std::wstring& path);

  struct Context;
  static void onDirectoryStart(Context* cx, std::wstring_view path);
  static void onDirectoryEnd(Context* cx, std::wstring_view path);
  static void onFile(Context* cx, std::wstring_view path, fs::file_time_type lwt);

  // sorts the subdirectories by name, case insensitive
  //
  void sortSubDirectories();

  // adds all the files and folders from the given archive recursively
  //
  void addFiles(
    FilesOrigin& origin, const BSA::Folder::Ptr& archiveFolder,
    fs::file_time_type archiveFileTime, const ArchiveInfo& archive);

  // creates the given subdirectory or returns an existing one
  //
  DirectoryEntry* getOrCreateSubDirectory(
    std::wstring_view name, OriginID originID);

  // splits `path` on separators and calls getOrCreateSubDirectory()
  // recursively
  //
  DirectoryEntry* getOrCreateSubDirectories(
    std::wstring_view path, OriginID originID);

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

#endif // MO_REGISTER_DIRECTORYENTRY_INCLUDED
