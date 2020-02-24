#include "originconnection.h"
#include "filesorigin.h"
#include "util.h"
#include <log.h>

namespace MOShared
{

using namespace MOBase;

OriginConnection::OriginConnection(std::shared_ptr<FileRegister> r)
  : m_nextID(0), m_register(r)
{
}

std::shared_ptr<OriginConnection> OriginConnection::create(
  std::shared_ptr<FileRegister> r)
{
  return std::shared_ptr<OriginConnection>(new OriginConnection(std::move(r)));
}

FilesOrigin& OriginConnection::getOrCreateOrigin(
  std::wstring_view name, const fs::path& path, int priority)
{
  std::unique_lock lock(m_mutex);

  // lookup by name
  auto itor = m_names.find(name);

  if (itor != m_names.end()) {
    // lookup by id
    auto itor2 = m_origins.find(itor->second);

    if (itor2 != m_origins.end()) {
      // already exists
      FilesOrigin& origin = itor2->second;
      origin.setEnabledFlag();
      return origin;
    }

    // found by name but not by id, this shouldn't happen
    log::error(
      "OriginConnection::getOrCreateOrigin(): "
      "origin '{}' found in names map but index {} not found; recreating",
      name, itor->second);
  }

  return createOriginNoLock(name, path, priority);
}

FilesOrigin& OriginConnection::createOrigin(
  std::wstring_view name, const fs::path& directory, int priority)
{
  std::scoped_lock lock(m_mutex);
  return createOriginNoLock(name, directory, priority);
}

bool OriginConnection::exists(std::wstring_view name)
{
  std::scoped_lock lock(m_mutex);
  return m_names.contains(name);
}

const FilesOrigin* OriginConnection::findByID(OriginID id) const
{
  std::scoped_lock lock(m_mutex);

  auto itor = m_origins.find(id);
  if (itor == m_origins.end()) {
    return nullptr;
  }

  return &itor->second;
}

const FilesOrigin* OriginConnection::findByName(std::wstring_view name) const
{
  std::scoped_lock lock(m_mutex);

  // find by name
  auto itor = m_names.find(name);
  if (itor == m_names.end()) {
    return nullptr;
  }

  // find by id
  auto itor2 = m_origins.find(itor->second);

  if (itor2 == m_origins.end()) {
    // found by name but not by id, this shouldn't happen
    log::error(
      "OriginConnection::findByName(): "
      "origin '{}' found in names map but index {} not found",
      name, itor->second);

    return nullptr;
  }

  return &itor2->second;
}

void OriginConnection::changePriorityLookup(int oldPriority, int newPriority)
{
  std::scoped_lock lock(m_mutex);

  // looking up old priority
  auto itor = m_priorities.find(oldPriority);

  if (itor == m_priorities.end()) {
    log::error(
      "cannot change origin priority lookup from {} to {}, "
      "not found in priority map",
      oldPriority, newPriority);

    return;
  }

  const OriginID index = itor->second;

  // removing old
  m_priorities.erase(itor);

  // setting new
  m_priorities.emplace(newPriority, index);
}

void OriginConnection::changeNameLookup(std::wstring_view oldName, std::wstring_view newName)
{
  std::scoped_lock lock(m_mutex);

  auto itor = m_names.find(oldName);

  if (itor == m_names.end()) {
    log::error(
      "cannot change origin name lookup from '{}' to '{}', "
      "not found in name map",
      oldName, newName);

    return;
  }

  const OriginID index = itor->second;

  // removing old
  m_names.erase(itor);

  // setting new
  m_names.emplace(newName, index);
}

std::shared_ptr<FileRegister> OriginConnection::getFileRegister() const
{
  return m_register.lock();
}

OriginID OriginConnection::createID()
{
  return m_nextID++;
}

FilesOrigin& OriginConnection::createOriginNoLock(
  std::wstring_view name, const fs::path& directory, int priority)
{
  const OriginID newID = createID();
  auto self = shared_from_this();

  // origins
  auto itor = m_origins.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(newID),
    std::forward_as_tuple(newID, name, directory, priority, self))
      .first;

  // names
  m_names.insert({std::wstring(name.begin(), name.end()), newID});

  // priorities
  m_priorities.insert({priority, newID});

  return itor->second;
}

} // namespace
