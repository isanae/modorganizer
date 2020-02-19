#ifndef MO_REGISTER_ORIGINCONNECTION_INCLUDED
#define MO_REGISTER_ORIGINCONNECTION_INCLUDED

#include "fileregisterfwd.h"

namespace MOShared
{

class OriginConnection
{
public:
  OriginConnection();

  // non-copyable
  OriginConnection(const OriginConnection&) = delete;
  OriginConnection& operator=(const OriginConnection&) = delete;

  std::pair<FilesOrigin&, bool> getOrCreate(
    const std::wstring &originName, const std::wstring &directory, int priority,
    const boost::shared_ptr<FileRegister>& fileRegister,
    const boost::shared_ptr<OriginConnection>& originConnection,
    DirectoryStats& stats);

  FilesOrigin& createOrigin(
    const std::wstring &originName, const std::wstring &directory, int priority,
    boost::shared_ptr<FileRegister> fileRegister,
    boost::shared_ptr<OriginConnection> originConnection);

  bool exists(const std::wstring &name);

  FilesOrigin &getByID(OriginID ID);
  const FilesOrigin* findByID(OriginID ID) const;
  FilesOrigin &getByName(const std::wstring &name);

  void changePriorityLookup(int oldPriority, int newPriority);

  void changeNameLookup(const std::wstring &oldName, const std::wstring &newName);

private:
  std::atomic<OriginID> m_NextID;
  std::map<OriginID, FilesOrigin> m_Origins;
  std::map<std::wstring, OriginID> m_OriginsNameMap;
  std::map<int, OriginID> m_OriginsPriorityMap;
  mutable std::mutex m_Mutex;

  OriginID createID();

  FilesOrigin& createOriginNoLock(
    const std::wstring &originName, const std::wstring &directory, int priority,
    boost::shared_ptr<FileRegister> fileRegister,
    boost::shared_ptr<OriginConnection> originConnection);
};

} // namespace

#endif // MO_REGISTER_ORIGINCONNECTION_INCLUDED
