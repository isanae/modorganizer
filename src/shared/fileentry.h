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
  void addOrigin(OriginID o, FILETIME time, const ArchiveInfo& archive);

  // removes the specified origin from the list of origins that contain this
  // file; returns true if that was the last origin, which is used elsewhere
  // to determine that this file should be removed entirely from the register
  //
  bool removeOrigin(OriginID origin);

  // sorts this file's origins by priority and makes the origin with the
  // highest priority the primary one
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
  const std::wstring &getName() const
  {
    return m_Name;
  }

  // primary origin
  //
  OriginID getOrigin() const
  {
    return m_Origin;
  }

  // the archive from the primary origin that contains this file, if any
  //
  const ArchiveInfo& getArchive() const
  {
    return m_Archive;
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
  DirectoryEntry *getParent()
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

private:
  // unique index
  FileIndex m_Index;

  // filename
  std::wstring m_Name;

  // primary origin
  OriginID m_Origin;

  // archive from the primary origin, if any
  ArchiveInfo m_Archive;

  // alternative origins and their archive, if any
  std::vector<OriginInfo> m_Alternatives;

  // parent directory
  DirectoryEntry *m_Parent;

  // last modified time
  FILETIME m_FileTime;

  // sizes
  std::optional<uint64_t> m_FileSize, m_CompressedFileSize;

  // protects m_Origin, m_Archive and m_Alternatives
  mutable std::mutex m_OriginsMutex;


  // creates a file with no origin
  //
  FileEntry(FileIndex index, std::wstring name, DirectoryEntry *parent);
};

} // namespace

#endif // MO_REGISTER_FILEENTRY_INCLUDED
