#ifndef MO_REGISTER_FILESORIGIN_INCLUDED
#define MO_REGISTER_FILESORIGIN_INCLUDED

#include "fileregisterfwd.h"

namespace MOShared
{

// represents a mod or the data directory, providing files to the tree
//
class FilesOrigin
{
public:
  FilesOrigin();

  FilesOrigin(
    OriginID ID, std::wstring_view name, const fs::path& path, int priority,
    std::shared_ptr<FileRegister> fileRegister,
    std::shared_ptr<OriginConnection> originConnection);

  // non-copyable
  FilesOrigin(const FilesOrigin&) = delete;
  FilesOrigin& operator=(const FilesOrigin&) = delete;

  // sets priority for this origin, but it will overwrite the existing mapping
  // for this priority, the previous origin will no longer be referenced
  void setPriority(int priority);

  int getPriority() const
  {
    return m_Priority;
  }

  void setName(const std::wstring &name);
  const std::wstring &getName() const
  {
    return m_Name;
  }

  OriginID getID() const
  {
    return m_ID;
  }

  const std::wstring &getPath() const
  {
    return m_Path;
  }

  std::vector<FileEntryPtr> getFiles() const;
  FileEntryPtr findFile(FileIndex index) const;

  void enable(bool enabled, DirectoryStats& stats);
  void enable(bool enabled);

  bool isDisabled() const
  {
    return m_Disabled;
  }

  void addFile(FileIndex index)
  {
    std::scoped_lock lock(m_Mutex);
    m_Files.insert(index);
  }

  void removeFile(FileIndex index);

  bool containsArchive(std::wstring archiveName);

private:
  OriginID m_ID;
  bool m_Disabled;
  std::set<FileIndex> m_Files;
  std::wstring m_Name;
  std::wstring m_Path;
  int m_Priority;
  std::weak_ptr<FileRegister> m_FileRegister;
  std::weak_ptr<OriginConnection> m_OriginConnection;
  mutable std::mutex m_Mutex;
};

} // namespace

#endif // MO_REGISTER_FILESORIGIN_INCLUDED
