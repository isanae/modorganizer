#ifndef MO_REGISTER_ORIGINCONNECTION_INCLUDED
#define MO_REGISTER_ORIGINCONNECTION_INCLUDED

#include "fileregisterfwd.h"

namespace MOShared
{

// central map of file origins, owned by a FileRegister
//
// maintains maps of:
//  - origin ID    => FilesOrigin
//  - origin name  => origin ID
//  - origin prio  => origin ID
//
class OriginConnection : public std::enable_shared_from_this<OriginConnection>
{
public:
  // empty origin connections
  //
  static std::shared_ptr<OriginConnection> create(
    std::shared_ptr<FileRegister> r);

  // non-copyable
  OriginConnection(const OriginConnection&) = delete;
  OriginConnection& operator=(const OriginConnection&) = delete;

  // returns an origin with the given name or creates one; if the origin
  // exists, its enabled flag is also set to true
  //
  FilesOrigin& getOrCreateOrigin(
    std::wstring_view name, const fs::path& path, int priority);

  // creates a new origin
  //
  FilesOrigin& createOrigin(
    std::wstring_view name, const fs::path& directory, int priority);

  // returns whether an origin with the given name exists
  //
  bool exists(std::wstring_view name);

  // returns the origin with the given id, or null if not found
  //
  const FilesOrigin* findByID(OriginID id) const;
  FilesOrigin* findByID(OriginID id)
  {
    return const_cast<FilesOrigin*>(std::as_const(*this).findByID(id));
  }

  // returns the origin with the given name, or null if not found
  //
  const FilesOrigin* findByName(std::wstring_view name) const;
  FilesOrigin* findByName(std::wstring_view name)
  {
    return const_cast<FilesOrigin*>(std::as_const(*this).findByName(name));
  }

  // moves the origin from `oldPriority` to `newPriority`; if an origin was
  // already present at `newPriority`, it is overwritten
  //
  void changePriorityLookup(int oldPriority, int newPriority);

  // moves the origin from `oldName` to `newName`; if an origin was already
  // present at `newName`, it is overwritten
  //
  void changeNameLookup(std::wstring_view oldName, std::wstring_view newName);

  // global file register
  //
  std::shared_ptr<FileRegister> getFileRegister() const;

private:
  // next origin id
  std::atomic<OriginID> m_nextID;

  // map of ids to origins
  std::map<OriginID, FilesOrigin> m_origins;

  // map of names to ids
  std::map<std::wstring, OriginID, std::less<>> m_names;

  // map of priorities to ids
  std::map<int, OriginID> m_priorities;

  // global register
  std::weak_ptr<FileRegister> m_register;

  // protects the maps
  mutable std::mutex m_mutex;


  // empty origin connection
  //
  OriginConnection(std::shared_ptr<FileRegister> r);

  // creates a new origin id
  //
  OriginID createID();

  // creates a file origin and adds it to the maps; doesn't lock the mutex
  //
  FilesOrigin& createOriginNoLock(
    std::wstring_view name, const fs::path& directory, int priority);
};

} // namespace

#endif // MO_REGISTER_ORIGINCONNECTION_INCLUDED
