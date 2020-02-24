#ifndef MO_REGISTER_FILESORIGIN_INCLUDED
#define MO_REGISTER_FILESORIGIN_INCLUDED

#include "fileregisterfwd.h"

namespace MOShared
{

// represents a mod or a pseudo-mod like the data directory; an origin
// maintains a list of files
//
class FilesOrigin
{
public:
  // creates an empty origin
  //
  FilesOrigin(
    OriginID ID, std::wstring_view name, const fs::path& path, int priority,
    std::shared_ptr<OriginConnection> oc);

  // non-copyable
  FilesOrigin(const FilesOrigin&) = delete;
  FilesOrigin& operator=(const FilesOrigin&) = delete;

  // sets the priority of this origin, this also calls
  // OriginConnection::changePriorityLookup(); note that if there is currently
  // an origin at the given priority, OriginConnection will lose track of it
  //
  void setPriority(int priority);

  // this origin's priority
  //
  int getPriority() const
  {
    return m_Priority;
  }

  // sets the name of this origin, this also calls
  // OriginConnection::changeNameLookup(); note that if there is currently
  // an origin with the given name, OriginConnection will lose track of it
  //
  void setName(std::wstring_view name);

  // this origin's name
  //
  const std::wstring& getName() const
  {
    return m_Name;
  }

  // this origin's unique id
  //
  OriginID getID() const
  {
    return m_ID;
  }

  // the path of the origin on the filesystem
  //
  const fs::path& getPath() const
  {
    return m_Path;
  }

  // list of files in this origin; this function is expensive because it has
  // to lookup every file in the register
  //
  std::vector<FileEntryPtr> getFiles() const;

  // sets whether this origin is enabled
  //
  void enable(bool enabled);

  // whether this origin is enabled
  //
  bool isEnabled() const
  {
    return m_Enabled;
  }

  // adds the given file to this origin
  //
  void addFile(FileIndex index);

  // removes the given file from this origin
  //
  void removeFile(FileIndex index);

  std::shared_ptr<OriginConnection> getOriginConnection() const;
  std::shared_ptr<FileRegister> getFileRegister() const;

private:
  // unique id
  OriginID m_ID;

  // whether the origin is enabled
  bool m_Enabled;

  // files in this origin
  std::set<FileIndex> m_Files;

  // origin name
  std::wstring m_Name;

  // path on the filesystem
  fs::path m_Path;

  // priority
  int m_Priority;

  // global register
  std::weak_ptr<OriginConnection> m_OriginConnection;

  // protects m_Files
  mutable std::mutex m_FilesMutex;
};

} // namespace

#endif // MO_REGISTER_FILESORIGIN_INCLUDED
