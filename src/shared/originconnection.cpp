#include "originconnection.h"
#include "filesorigin.h"
#include "util.h"
#include <log.h>

namespace MOShared
{

using namespace MOBase;

OriginConnection::OriginConnection()
  : m_NextID(0)
{
}

std::shared_ptr<OriginConnection> OriginConnection::create()
{
  return std::shared_ptr<OriginConnection>(new OriginConnection);
}

std::pair<FilesOrigin&, bool> OriginConnection::getOrCreateOrigin(
  std::wstring_view name, const fs::path& path, int priority)
{
  std::unique_lock lock(m_Mutex);

  auto itor = m_OriginsNameMap.find(name);

  if (itor != m_OriginsNameMap.end()) {
    auto itor2 = m_Origins.find(itor->second);

    // todo: log not found

    if (itor2 != m_Origins.end()) {
      FilesOrigin& origin = itor2->second;
      lock.unlock();
      origin.enable(true);
      return {origin, false};
    }
  }

  FilesOrigin& origin = createOriginNoLock(name, path, priority);
  return {origin, true};
}

FilesOrigin& OriginConnection::createOrigin(
  std::wstring_view name, const fs::path& directory, int priority)
{
  std::scoped_lock lock(m_Mutex);
  return createOriginNoLock(name, directory, priority);
}

bool OriginConnection::exists(std::wstring_view name)
{
  std::scoped_lock lock(m_Mutex);
  return m_OriginsNameMap.find(name) != m_OriginsNameMap.end();
}

//FilesOrigin& OriginConnection::getByID(OriginID id)
//{
//  std::scoped_lock lock(m_Mutex);
//  return m_Origins[id];
//}

const FilesOrigin* OriginConnection::findByID(OriginID id) const
{
  std::scoped_lock lock(m_Mutex);

  auto itor = m_Origins.find(id);

  if (itor == m_Origins.end()) {
    return nullptr;
  } else {
    return &itor->second;
  }
}

const FilesOrigin* OriginConnection::findByName(std::wstring_view name) const
{
  std::scoped_lock lock(m_Mutex);

  auto iter = m_OriginsNameMap.find(name);
  if (iter == m_OriginsNameMap.end()) {
    return nullptr;
  }

  auto iter2 = m_Origins.find(iter->second);
  if (iter2 == m_Origins.end()) {
    return nullptr;
  }

  return &iter2->second;
}

void OriginConnection::changePriorityLookup(int oldPriority, int newPriority)
{
  std::scoped_lock lock(m_Mutex);

  auto iter = m_OriginsPriorityMap.find(oldPriority);

  if (iter != m_OriginsPriorityMap.end()) {
    OriginID idx = iter->second;
    m_OriginsPriorityMap.erase(iter);
    m_OriginsPriorityMap[newPriority] = idx;
  }
}

void OriginConnection::changeNameLookup(std::wstring_view oldName, std::wstring_view newName)
{
  std::scoped_lock lock(m_Mutex);

  auto iter = m_OriginsNameMap.find(oldName);

  if (iter == m_OriginsNameMap.end()) {
    log::error(
      "cannot change origin name lookup from '{}' to '{}', "
      "not found in name map",
      oldName, newName);

    return;
  }

  OriginID index = iter->second;

  m_OriginsNameMap.erase(iter);
  m_OriginsNameMap.emplace(newName, index);
}

std::shared_ptr<FileRegister> OriginConnection::getFileRegister() const
{
  return m_FileRegister.lock();
}

OriginID OriginConnection::createID()
{
  return m_NextID++;
}

FilesOrigin& OriginConnection::createOriginNoLock(
  std::wstring_view name, const fs::path& directory, int priority)
{
  OriginID newID = createID();
  auto self = shared_from_this();

  auto itor = m_Origins.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(newID),
    std::forward_as_tuple(newID, name, directory, priority, self))
      .first;

  m_OriginsNameMap.insert({std::wstring(name.begin(), name.end()), newID});
  m_OriginsPriorityMap.insert({priority, newID});

  return itor->second;
}

} // namespace
