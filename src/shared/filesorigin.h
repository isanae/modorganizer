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
    OriginID ID, const OriginData& data, std::shared_ptr<OriginConnection> oc);

  // non-copyable
  FilesOrigin(const FilesOrigin&) = delete;
  FilesOrigin& operator=(const FilesOrigin&) = delete;

  // sets the priority of this origin
  //
  void setPriority(int priority);

  // this origin's priority
  //
  int getPriority() const
  {
    return m_priority;
  }

  // sets the name of this origin, also changes the last component of the path
  // to the same value
  //
  // this also calls OriginConnection::changeNameLookup(); note that if there
  // is currently an origin with the given name, OriginConnection will lose
  // track of it
  //
  void setName(std::wstring_view name);

  // this origin's name
  //
  const std::wstring& getName() const
  {
    return m_name;
  }

  // this origin's unique id
  //
  OriginID getID() const
  {
    return m_id;
  }

  // the path of the origin on the filesystem
  //
  const fs::path& getPath() const
  {
    return m_path;
  }

  // list of files in this origin; this function is expensive because it has
  // to lookup every file in the register
  //
  std::vector<FileEntryPtr> getFiles() const;

  // list of files indices in this origin
  //
  const std::set<FileIndex>& getFileIndices() const
  {
    return m_files;
  }

  // removes all of this origin's files from the register and marks the origin
  // as being disabled
  //
  void clearFilesInternal();

  // adds the given file to this origin
  //
  void addFileInternal(FileIndex index);

  // removes the given file from this origin
  //
  void removeFileInternal(FileIndex index);


  // global origin connection
  //
  std::shared_ptr<OriginConnection> getOriginConnection() const;

  // global file register
  //
  std::shared_ptr<FileRegister> getFileRegister() const;


  // returns a string that represents this file, such as "name:id";
  // useful for logging
  //
  std::wstring debugName() const;

private:
  // unique id
  OriginID m_id;

  // files in this origin
  std::set<FileIndex> m_files;

  // origin name
  std::wstring m_name;

  // path on the filesystem
  fs::path m_path;

  // priority
  int m_priority;

  // global register
  std::weak_ptr<OriginConnection> m_originConnection;

  // protects m_Files
  mutable std::mutex m_filesMutex;
};

} // namespace

#endif // MO_REGISTER_FILESORIGIN_INCLUDED
