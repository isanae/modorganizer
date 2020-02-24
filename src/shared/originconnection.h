#ifndef MO_REGISTER_ORIGINCONNECTION_INCLUDED
#define MO_REGISTER_ORIGINCONNECTION_INCLUDED

#include "fileregisterfwd.h"

namespace MOShared
{

class OriginConnection : public std::enable_shared_from_this<OriginConnection>
{
public:
  static std::shared_ptr<OriginConnection> create();

  // non-copyable
  OriginConnection(const OriginConnection&) = delete;
  OriginConnection& operator=(const OriginConnection&) = delete;

  std::pair<FilesOrigin&, bool> getOrCreateOrigin(
    std::wstring_view name, const fs::path& path, int priority);

  FilesOrigin& createOrigin(
    std::wstring_view name, const fs::path& directory, int priority);

  bool exists(std::wstring_view name);

  const FilesOrigin* findByID(OriginID id) const;
  FilesOrigin* findByID(OriginID id)
  {
    return const_cast<FilesOrigin*>(std::as_const(*this).findByID(id));
  }

  const FilesOrigin* findByName(std::wstring_view name) const;
  FilesOrigin* findByName(std::wstring_view name)
  {
    return const_cast<FilesOrigin*>(std::as_const(*this).findByName(name));
  }

  void changePriorityLookup(int oldPriority, int newPriority);

  void changeNameLookup(std::wstring_view oldName, std::wstring_view newName);

  std::shared_ptr<FileRegister> getFileRegister() const;

private:
  std::atomic<OriginID> m_NextID;
  std::map<OriginID, FilesOrigin> m_Origins;
  std::map<std::wstring, OriginID, std::less<>> m_OriginsNameMap;
  std::map<int, OriginID> m_OriginsPriorityMap;
  std::weak_ptr<FileRegister> m_FileRegister;
  mutable std::mutex m_Mutex;

  OriginConnection();

  OriginID createID();

  FilesOrigin& createOriginNoLock(
    std::wstring_view name, const fs::path& directory, int priority);
};

} // namespace

#endif // MO_REGISTER_ORIGINCONNECTION_INCLUDED
