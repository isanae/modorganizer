#ifndef MO_REGISTER_FILEENTRY_INCLUDED
#define MO_REGISTER_FILEENTRY_INCLUDED

#include "fileregisterfwd.h"

// represents a file inside a DirectoryEntry
//
// each file has a unique index, which is generated in
// FileRegister::createFile()
//
// files have a primary origin (the mod that won the conflict) and alternative
// origins (other mods that provide the file but are lower in the mod order)
//
// if a file comes from an archive, it remembers the archive that has the
// plugin with the highest load order
//
class FileEntry
{
public:
  static FileEntryPtr create(
    FileIndex index, std::wstring name, DirectoryEntry* parent);

  // non-copyable
  FileEntry(const FileEntry&) = delete;
  FileEntry& operator=(const FileEntry&) = delete;

  // unique index of this file in the FileRegister
  //
  FileIndex getIndex() const
  {
    return m_index;
  }

  // filename
  //
  const std::wstring& getName() const
  {
    return m_name;
  }

  // the list of origins, sorted by priority, that also provide this file but
  // with a lower priority
  //
  const std::vector<OriginInfo>& getAlternatives() const
  {
    return m_alternatives;
  }

  // primary origin
  //
  OriginID getOrigin() const
  {
    return m_origin.originID;
  }

  // the archive from the primary origin that contains this file, if any
  //
  const ArchiveInfo& getArchive() const
  {
    return m_origin.archive;
  }

  // returns the directory that contains this file
  //
  DirectoryEntry* getParent()
  {
    return m_parent;
  }

  // returns the absolute path of this file; note that the file might not
  // actually exist on the filesystem if it's from an archive
  //
  // if `originID` is `InvalidOriginID`, uses the primary origin; if not, uses
  // the given origin, returns an empty string if the file doesn't exist in
  // that origin
  //
  fs::path getFullPath(OriginID originID=InvalidOriginID) const;

  // returns the path of this file relative to the Data directory (excludes
  // the Data directory itself)
  //
  fs::path getRelativePath() const;


  // whether this file is found in the given archive
  //
  bool existsInArchive(std::wstring_view archiveName) const;

  // whether the primary origin has this file in an archive
  //
  bool isFromArchive() const;


  // adds the given origin to this file
  //
  void addOriginInternal(
    const OriginInfo& newOrigin, std::optional<FILETIME> time);

  // removes the specified origin from the list of origins that contain this
  // file; returns true if that was the last origin, which is used elsewhere
  // to determine that this file should be removed entirely from the register
  //
  bool removeOriginInternal(OriginID origin);

  // sorts this file's origins by priority and makes the origin with the
  // highest priority the primary one
  //
  // this file's origins are normally kept sorted at all times when adding or
  // removing  them, but the origins themselves might change priorities when
  // the user modifies the mod list
  //
  // this re-checks the priorities of all origins and re-picks the highest one
  // if necessary
  //
  void sortOrigins();


  // sets the last modified time of this file
  //
  void setFileTime(FILETIME fileTime)
  {
    m_fileTime = fileTime;
  }

  // last modified time of the file; if the file is from an archive, this is
  // the last modified time of the archive; may be empty if the file doesn't
  // exist
  //
  std::optional<FILETIME> getFileTime() const
  {
    return m_fileTime;
  }

  // sets the size of this file
  //
  void setFileSize(uint64_t size)
  {
    m_fileSize = size;
  }

  // the size of this file, can be unavailable; if the file is from an archive,
  // this is the uncompressed file size if getCompressedFileSize() is not empty
  //
  std::optional<uint64_t> getFileSize() const
  {
    return m_fileSize;
  }

  // sets the compressed size of this file, used only when the file is from an
  // archive
  //
  void setCompressedFileSize(uint64_t compressedSize)
  {
    m_compressedFileSize = compressedSize;
  }

  // the compressed size of this file, can be empty if the file is not from an
  // archive or if the archive doesn't know the compressed size
  //
  std::optional<uint64_t> getCompressedFileSize() const
  {
    return m_compressedFileSize;
  }

  // returns a string that represents this file, such as "filename:index";
  // useful for logging
  //
  std::wstring debugName() const;

private:
  // unique index
  FileIndex m_index;

  // filename
  std::wstring m_name;

  // primary origin
  OriginInfo m_origin;

  // alternative origins and their archive, if any; always sorted by priority
  std::vector<OriginInfo> m_alternatives;

  // parent directory
  DirectoryEntry* m_parent;

  // last modified time
  std::optional<FILETIME> m_fileTime;

  // sizes
  std::optional<uint64_t> m_fileSize, m_compressedFileSize;

  // protects m_origin and m_alternatives
  mutable std::mutex m_originsMutex;


  // creates a file with no origin
  //
  FileEntry(FileIndex index, std::wstring name, DirectoryEntry* parent);

  // returns whether the given origin should replace the current primary origin
  // of this file
  //
  bool shouldReplacePrimaryOrigin(const OriginInfo& newOrigin) const;

  // sets the primary origin of this file to the given one; if this file
  // already has a primary origin, it is moved to the alternatives
  //
  void setPrimaryOrigin(
    const OriginInfo& newOrigin, std::optional<FILETIME> time);

  // adds the given origin to the alternatives list
  //
  void addAlternativeOrigin(const OriginInfo& newOrigin);

  // three-way comparison of the given origins based on priorities and
  // whether they're from archives; -1 if `a` has a lower prio, +1 if `a` has
  // a higher prio, 0 if they're equal
  //
  int comparePriorities(const OriginInfo& a, const OriginInfo& b) const;

  // returns the alternative when the given id, or m_alternatives.end()
  //
  std::vector<OriginInfo>::const_iterator findAlternativeByID(OriginID id) const;

  // temporary, for debugging; breaks into the debugger if the alternatives are
  // not sorted property
  //
  void assertAlternativesSorted() const;
};

#endif // MO_REGISTER_FILEENTRY_INCLUDED
