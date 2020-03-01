#ifndef MO_REGISTER_ORIGINCONNECTION_INCLUDED
#define MO_REGISTER_ORIGINCONNECTION_INCLUDED

#include "fileregisterfwd.h"

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
  FilesOrigin& getOrCreateOrigin(const OriginData& data);

  // creates a new origin
  //
  FilesOrigin& createOrigin(const OriginData& data);

  // returns the Data origin
  //
  FilesOrigin& getDataOrigin();

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

  // moves the origin from `oldName` to `newName`; if an origin was already
  // present at `newName`, it is overwritten
  //
  void changeNameLookupInternal(
    std::wstring_view oldName, std::wstring_view newName);

  // global file register
  //
  std::shared_ptr<FileRegister> getFileRegister() const;

private:
  using IndicesMap = std::map<OriginID, FilesOrigin>;
  using NamesMap = std::map<std::wstring, OriginID, std::less<>>;

  // next origin id
  std::atomic<OriginID> m_nextID;

  // map of ids to origins
  IndicesMap m_origins;

  // map of names to ids
  NamesMap m_names;

  // global register
  std::weak_ptr<FileRegister> m_register;

  // protects the maps
  mutable std::mutex m_mutex;


  // empty origin connection
  //
  OriginConnection(std::shared_ptr<FileRegister> r);

  // creates the Data origin
  //
  FilesOrigin& createDataOriginNoLock();

  // creates a new origin id
  //
  OriginID createID();

  // creates a file origin and adds it to the maps; doesn't lock the mutex
  //
  FilesOrigin& createOriginNoLock(const OriginData& data);

  // fixes problems when changeNameLookupInternal() is called and its target
  // already exists
  //
  void handleRenameDiscrepancies(
    std::wstring_view oldName, std::wstring_view newName,
    FileIndex index, NamesMap::iterator newItor);
};

#endif // MO_REGISTER_ORIGINCONNECTION_INCLUDED
