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
    std::wstring_view name, const fs::path& path, int priority,
    const std::shared_ptr<FileRegister>& fileRegister);

  FilesOrigin& createOrigin(
    std::wstring_view name, const fs::path& directory, int priority,
    std::shared_ptr<FileRegister> fileRegister);

  bool exists(std::wstring_view name);
  FilesOrigin& getByID(OriginID id);
  FilesOrigin& getByName(std::wstring_view name);
  const FilesOrigin* findByID(OriginID id) const;

  void changePriorityLookup(int oldPriority, int newPriority);

  void changeNameLookup(const std::wstring &oldName, const std::wstring &newName);

private:
  std::atomic<OriginID> m_NextID;
  std::map<OriginID, FilesOrigin> m_Origins;
  std::map<std::wstring, OriginID, std::less<>> m_OriginsNameMap;
  std::map<int, OriginID> m_OriginsPriorityMap;
  mutable std::mutex m_Mutex;

  OriginConnection();

  OriginID createID();

  FilesOrigin& createOriginNoLock(
    std::wstring_view name, const fs::path& directory, int priority,
    std::shared_ptr<FileRegister> fileRegister);
};

} // namespace

#endif // MO_REGISTER_ORIGINCONNECTION_INCLUDED
