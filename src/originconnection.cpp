#include "originconnection.h"
#include "filesorigin.h"
#include "shared/util.h"
#include <iplugingame.h>
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

FilesOrigin& OriginConnection::getOrCreateOrigin(const OriginData& data)
{
  std::unique_lock lock(m_mutex);

  // lookup by name
  auto itor = m_names.find(data.name);

  if (itor != m_names.end()) {
    // lookup by id
    auto itor2 = m_origins.find(itor->second);

    if (itor2 != m_origins.end()) {
      // already exists
      return itor2->second;
    }

    // found by name but not by id, this shouldn't happen
    log::error(
      "OriginConnection::getOrCreateOrigin(): "
      "origin '{}' found in names map but index {} not found; recreating",
      data.name, itor->second);
  }

  return createOriginNoLock(data);
}

FilesOrigin& OriginConnection::createOrigin(const OriginData& data)
{
  std::scoped_lock lock(m_mutex);
  return createOriginNoLock(data);
}

FilesOrigin& OriginConnection::getDataOrigin()
{
  std::scoped_lock lock(m_mutex);

  auto itor = m_origins.find(DataOriginID);

  if (itor != m_origins.end()) {
    return itor->second;
  }

  const auto* game = qApp->property("managed_game").value<IPluginGame*>();
  const auto dir = game->dataDirectory().absolutePath();
  const auto dirW = QDir::toNativeSeparators(dir).toStdWString();

  return createOriginNoLock({L"data", dirW, DataOriginID});
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

void OriginConnection::changeNameLookupInternal(
  std::wstring_view oldName, std::wstring_view newName)
{
  std::scoped_lock lock(m_mutex);

  // this moves a mod from oldName to newName in the m_names map; if there's
  // no mod at m_names[oldName] or if there's already a mod at
  // m_names[newName], something's wrong

  auto oldItor = m_names.find(oldName);

  if (oldItor == m_names.end()) {
    // oldName not found
    log::error(
      "cannot change origin name lookup from '{}' to '{}', "
      "not found in name map",
      oldName, newName);

    return;
  }

  // index of the mod being moved
  const OriginID index = oldItor->second;

  // look for the new name in the map, it shouldn't be there; the iterator can
  // also be used in emplace() as a hint below
  auto newItor = m_names.lower_bound(newName);

  if (newItor != m_names.end() && newItor->first == newName) {
    // this shouldn't happen, it means that there is already a mod with the
    // new name
    //
    // because the mod index at m_names[newName] will be overwritten, m_origins
    // will end up desynchronized because it will still have the old index in
    // it, so it has to be removed
    handleRenameDiscrepancies(oldName, newName, index, newItor);

    // the already existing mod should now be gone from both maps and the
    // renaming can proceed
  }

  if (newItor == oldItor) {
    newItor = m_names.end();
  }

  // removing old name
  m_names.erase(oldItor);

  // adding new name
  m_names.emplace_hint(newItor, newName, index);
}

void OriginConnection::handleRenameDiscrepancies(
  std::wstring_view oldName, std::wstring_view newName,
  FileIndex index, NamesMap::iterator newItor)
{
  // there's already a mod in m_names[newName] and it's about to be overwritten
  // by the mod at m_names[oldName]; make sure the overwritten mod is also
  // removed from m_origins because it doesn't seem to exist anymore

  if (newItor->second == index) {
    // not only does the name already exist, but the index is the same;
    // something's seriously wrong; the index shouldn't be removed because it
    // still exists

    log::warn(
      "while changing origin {} name from '{}' to '{}', there's already an "
      "origin with the same index and name; overwriting",
      index, oldName, newName);

    return;
  }

  log::warn(
    "while changing origin {} name from '{}' to '{}', there's already an "
    "origin with the new name, index is {}; overwriting",
    index, oldName, newName, newItor->second);

  // look for the index in m_origins
  auto itor = m_origins.find(newItor->second);

  if (itor == m_origins.end()) {
    // heck, the maps are really desynced, not much that can be done at this
    // point
    log::error(
      "...but the index {} wasn't found in the origins map; ignoring",
      newItor->second);

    return;
  }

  // remove the now duplicated mod
  m_names.erase(newItor);

  // remove the overwritten mod from the map
  m_origins.erase(itor);
}

std::shared_ptr<FileRegister> OriginConnection::getFileRegister() const
{
  return m_register.lock();
}

OriginID OriginConnection::createID()
{
  return m_nextID++;
}

FilesOrigin& OriginConnection::createOriginNoLock(const OriginData& data)
{
  const OriginID newID = createID();
  auto self = shared_from_this();

  // origins
  auto r = m_origins.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(newID),
    std::forward_as_tuple(newID, data, self));

  // names
  m_names.emplace(std::wstring(data.name.begin(), data.name.end()), newID);

  return r.first->second;
}

} // namespace
