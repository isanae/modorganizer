#ifndef MO_REGISTER_FILESREGISTER_INCLUDED
#define MO_REGISTER_FILESREGISTER_INCLUDED

#include "fileregisterfwd.h"
#include <mutex>

namespace MOShared
{

// central register for all files; there is only one FileRegister, shared with
// all DirectoryEntry objects
//
class FileRegister
{
public:
  static std::shared_ptr<FileRegister> create();

  // non-copyable
  FileRegister(const FileRegister&) = delete;
  FileRegister& operator=(const FileRegister&) = delete;

  bool indexValid(FileIndex index) const;

  FileEntryPtr createFile(
    std::wstring name, DirectoryEntry *parent, DirectoryStats& stats);

  FileEntryPtr getFile(FileIndex index) const;

  size_t highestCount() const
  {
    std::scoped_lock lock(m_Mutex);
    return m_Files.size();
  }

  std::shared_ptr<OriginConnection> getOriginConnection() const;

  bool removeFile(FileIndex index);
  void removeOriginMulti(std::set<FileIndex> indices, OriginID originID);

  void sortOrigins();

private:
  using FileMap = std::deque<FileEntryPtr>;

  mutable std::mutex m_Mutex;
  FileMap m_Files;
  std::shared_ptr<OriginConnection> m_OriginConnection;
  std::atomic<FileIndex> m_NextIndex;

  FileRegister();

  FileIndex generateIndex();
};

} // namespace

#endif // MO_REGISTER_FILESREGISTER_INCLUDED
