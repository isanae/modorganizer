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
  std::wstring_view name, const fs::path& path, int priority,
  const std::shared_ptr<FileRegister>& fr)
{
  std::unique_lock lock(m_Mutex);

  auto itor = m_OriginsNameMap.find(name);

  if (itor == m_OriginsNameMap.end()) {
    FilesOrigin& origin = createOriginNoLock(name, path, priority, fr);
    return {origin, true};
  } else {
    FilesOrigin& origin = m_Origins[itor->second];
    lock.unlock();

    origin.enable(true);
    return {origin, false};
  }
}

FilesOrigin& OriginConnection::createOrigin(
  std::wstring_view name, const fs::path& directory, int priority,
  std::shared_ptr<FileRegister> fr)
{
  std::scoped_lock lock(m_Mutex);
  return createOriginNoLock(name, directory, priority, fr);
}

bool OriginConnection::exists(std::wstring_view name)
{
  std::scoped_lock lock(m_Mutex);
  return m_OriginsNameMap.find(name) != m_OriginsNameMap.end();
}

FilesOrigin& OriginConnection::getByID(OriginID id)
{
  std::scoped_lock lock(m_Mutex);
  return m_Origins[id];
}

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

FilesOrigin& OriginConnection::getByName(std::wstring_view name)
{
  std::scoped_lock lock(m_Mutex);

  auto iter = m_OriginsNameMap.find(name);

  if (iter != m_OriginsNameMap.end()) {
    return m_Origins[iter->second];
  } else {
    std::ostringstream stream;

    stream
      << QObject::tr("invalid origin name: ").toStdString()
      << ToString(std::wstring(name.begin(), name.end()), true);

    throw std::runtime_error(stream.str());
  }
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

void OriginConnection::changeNameLookup(const std::wstring &oldName, const std::wstring &newName)
{
  std::scoped_lock lock(m_Mutex);

  auto iter = m_OriginsNameMap.find(oldName);

  if (iter != m_OriginsNameMap.end()) {
    OriginID idx = iter->second;
    m_OriginsNameMap.erase(iter);
    m_OriginsNameMap[newName] = idx;
  } else {
    log::error(QObject::tr("failed to change name lookup from {} to {}").toStdString(), oldName, newName);
  }
}

OriginID OriginConnection::createID()
{
  return m_NextID++;
}

FilesOrigin& OriginConnection::createOriginNoLock(
  std::wstring_view name, const fs::path& directory, int priority,
  std::shared_ptr<FileRegister> fr)
{
  OriginID newID = createID();
  auto self = shared_from_this();

  auto itor = m_Origins.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(newID),
    std::forward_as_tuple(newID, name, directory, priority, fr, self))
      .first;

  m_OriginsNameMap.insert({std::wstring(name.begin(), name.end()), newID});
  m_OriginsPriorityMap.insert({priority, newID});

  return itor->second;
}

} // namespace
