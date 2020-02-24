#ifndef MO_REGISTER_FILEENTRY_INCLUDED
#define MO_REGISTER_FILEENTRY_INCLUDED

#include "fileregisterfwd.h"

namespace MOShared
{

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
    return m_Index;
  }

  // adds the given origin to this file
  //
  void addOrigin(const OriginInfo& newOrigin, FILETIME time);

  // removes the specified origin from the list of origins that contain this
  // file; returns true if that was the last origin, which is used elsewhere
  // to determine that this file should be removed entirely from the register
  //
  bool removeOrigin(OriginID origin);

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

  // the list of origins, sorted by priority, that also provide this file but
  // with a lower priority
  //
  const std::vector<OriginInfo>& getAlternatives() const
  {
    return m_Alternatives;
  }

  // filename
  //
  const std::wstring& getName() const
  {
    return m_Name;
  }

  // primary origin
  //
  OriginID getOrigin() const
  {
    return m_Origin.originID;
  }

  // the archive from the primary origin that contains this file, if any
  //
  const ArchiveInfo& getArchive() const
  {
    return m_Origin.archive;
  }

  // whether this file is found in the given archive
  //
  bool existsInArchive(std::wstring_view archiveName) const;

  // whether the primary origin has this file in an archive
  //
  bool isFromArchive() const;

  // returns the absolute path of this file; note that the file might not
  // actually exist on the filesystem if it's from an archive
  //
  // if `originID` is `InvalidOriginID`, uses the primary origin; if not, uses
  // the given origin, returns an empty string if the file doesn't exist in
  // that origin
  //
  std::wstring getFullPath(OriginID originID=InvalidOriginID) const;

  // returns the path of this file relative to the Data directory (excludes
  // the Data directory itself)
  //
  std::wstring getRelativePath() const;

  // returns the directory that contains this file
  //
  DirectoryEntry* getParent()
  {
    return m_Parent;
  }


  // sets the last modified time of this file
  //
  void setFileTime(FILETIME fileTime)
  {
    m_FileTime = fileTime;
  }

  // last modified time of the file; if the file is from an archive, this is
  // the last modified time of the archive
  //
  FILETIME getFileTime() const
  {
    return m_FileTime;
  }

  // sets the size of this file
  //
  void setFileSize(uint64_t size)
  {
    m_FileSize = size;
  }

  // the size of this file, can be unavailable; if the file is from an archive,
  // this is the uncompressed file size if getCompressedFileSize() is not empty
  //
  std::optional<uint64_t> getFileSize() const
  {
    return m_FileSize;
  }

  // sets the compressed size of this file, used only when the file is from an
  // archive
  //
  void setCompressedFileSize(uint64_t compressedSize)
  {
    m_CompressedFileSize = compressedSize;
  }

  // the compressed size of this file, can be empty if the file is not from an
  // archive or if the archive doesn't know the compressed size
  //
  std::optional<uint64_t> getCompressedFileSize() const
  {
    return m_CompressedFileSize;
  }

  // returns a string that represents this file, such as "filename:index";
  // useful for logging
  //
  std::wstring debugName() const;

private:
  // unique index
  FileIndex m_Index;

  // filename
  std::wstring m_Name;

  // primary origin
  OriginInfo m_Origin;

  // alternative origins and their archive, if any; always sorted by priority
  std::vector<OriginInfo> m_Alternatives;

  // parent directory
  DirectoryEntry* m_Parent;

  // last modified time
  FILETIME m_FileTime;

  // sizes
  std::optional<uint64_t> m_FileSize, m_CompressedFileSize;

  // protects m_Origin and m_Alternatives
  mutable std::mutex m_OriginsMutex;


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
  void setPrimaryOrigin(const OriginInfo& newOrigin, FILETIME fileTime);

  // adds the given origin to the alternatives list
  //
  void addAlternativeOrigin(const OriginInfo& newOrigin);

  // three-way comparison of the given origins based on priorities and
  // whether they're from archives
  //
  int comparePriorities(const OriginInfo& a, const OriginInfo& b) const;

  // returns the alternative when the given id, or m_Alternatives.end()
  //
  std::vector<OriginInfo>::const_iterator findAlternativeByID(OriginID id) const;

  // temporary, for debugging; breaks into the debugger if the alternatives are
  // not sorted property
  //
  void assertAlternativesSorted() const;
};

} // namespace

#endif // MO_REGISTER_FILEENTRY_INCLUDED
