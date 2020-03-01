#ifndef MO_REGISTER_FILESREGISTER_INCLUDED
#define MO_REGISTER_FILESREGISTER_INCLUDED

#include "fileregisterfwd.h"
#include <mutex>

// central register for all files; there is only one FileRegister, owned by
// DirectoryStructure and shared with all DirectoryEntry objects
//
// the register is actually a deque and the index of an element is the actual
// file index
//
// this makes adding and removing much faster than using a map, but it has
// several consequences:
//   - repeatedly adding and removing files creates holes in the deque because
//     the elements are simply reset to null pointers
//   - memory usage will continually increase until a full refresh is done
//
class FileRegister
{
public:
  // empty register
  //
  static std::shared_ptr<FileRegister> create();

  // non-copyable
  FileRegister(const FileRegister&) = delete;
  FileRegister& operator=(const FileRegister&) = delete;

  // whether a file with the given index exists
  //
  bool fileExists(FileIndex index) const;

  // returns the file having the given index, if any
  //
  FileEntryPtr getFile(FileIndex index) const;

  // number of files in this register
  //
  std::size_t fileCount() const
  {
    std::scoped_lock lock(m_mutex);
    return m_fileCount;
  }


  // creates a new FileEntry, adds it to the register and returns it
  //
  FileEntryPtr createFileInternal(
    std::wstring name, DirectoryEntry* parent);

  // 1) creates the given file if it doesn't exist,
  // 2) adds it to `parent`,
  // 3) adds the given origin to the file, and
  // 4) adds the file to the origin
  //
  FileEntryPtr addFile(
    DirectoryEntry& parent, std::wstring_view name, FilesOrigin& origin,
    FILETIME fileTime, const ArchiveInfo& archive);

  // 1) removes the file from the register,
  // 2) removes the file from all of its origins, and
  // 3) removes the file from its parent directory
  //
  void removeFile(FileIndex index);

  // 1) removes the file from the old origin and vice-versa, and
  // 2) adds the file to the old origin and vice-versa
  //
  void changeFileOrigin(FileEntry& file, FilesOrigin& from, FilesOrigin& to);

  // for each file in the given origin:
  //  1) removes the given origin from the file,
  //  2) removes the file from its parent directory
  //  3) if the file has no more origins, removes it from the registry
  //
  // then clears all files from the origin itself
  //
  void disableOrigin(FilesOrigin& o);

  // sorts the origins of every file and re-checks which one is the primary
  //
  void sortOrigins();


  // origin connection, manages the list of origins
  //
  std::shared_ptr<OriginConnection> getOriginConnection() const;

private:
  using FileMap = std::deque<FileEntryPtr>;

  // files in the register
  FileMap m_files;

  // origins
  std::shared_ptr<OriginConnection> m_originConnection;

  // total number of files, has to be kept separately because m_files never
  // shrinks
  std::size_t m_fileCount;

  // protects m_files and m_fileCount
  mutable std::mutex m_mutex;


  // empty register
  //
  FileRegister();
};

#endif // MO_REGISTER_FILESREGISTER_INCLUDED
