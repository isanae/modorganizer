#include "filesorigin.h"
#include "originconnection.h"
#include "fileregister.h"
#include "fileentry.h"
#include <log.h>

namespace MOShared
{

using namespace MOBase;

FilesOrigin::FilesOrigin(
  OriginID ID, std::wstring_view name, const fs::path& path, int prio,
  std::shared_ptr<OriginConnection> oc) :
    m_ID(ID), m_Enabled(true), m_Name(name), m_Path(path), m_Priority(prio),
    m_OriginConnection(oc)
{
}

void FilesOrigin::setPriority(int priority)
{
  if (auto oc=m_OriginConnection.lock()) {
    oc->changePriorityLookup(m_Priority, priority);
  }

  m_Priority = priority;
}

void FilesOrigin::setName(std::wstring_view name)
{
  if (auto oc=m_OriginConnection.lock()) {
    oc->changeNameLookup(m_Name, name);
  }

  // the path should always match the name
  if (m_Path.filename().native() != m_Name) {
    log::warn(
      "files origin '{}': path '{}' doesn't end with name",
      m_Name, m_Path.native());
  }

  m_Path = m_Path.parent_path() / name;
  m_Name = name;
}

std::vector<FileEntryPtr> FilesOrigin::getFiles() const
{
  std::vector<FileEntryPtr> v;

  auto oc = m_OriginConnection.lock();
  if (!oc) {
    return v;
  }

  auto fr = oc->getFileRegister();
  if (!fr) {
    return v;
  }

  {
    std::scoped_lock lock(m_FilesMutex);

    for (const FileIndex& index : m_Files) {
      if (auto f=fr->getFile(index)) {
        v.push_back(f);
      }
    }
  }

  return v;
}

void FilesOrigin::disable()
{
  std::set<FileIndex> files;

  // stealing the files
  {
    std::scoped_lock lock(m_FilesMutex);
    files = std::move(m_Files);
    m_Files.clear();
  }

  // removing files
  if (auto fr=getFileRegister()) {
    fr->removeOrigin(files, m_ID);
  }

  m_Enabled = false;
}

void FilesOrigin::setEnabledFlag()
{
  m_Enabled = true;
}

void FilesOrigin::addFile(FileIndex index)
{
  std::scoped_lock lock(m_FilesMutex);
  m_Files.insert(index);
}

void FilesOrigin::removeFile(FileIndex index)
{
  std::scoped_lock lock(m_FilesMutex);

  auto itor = m_Files.find(index);

  if (itor == m_Files.end()) {
    // just logging

    FileEntryPtr f;

    if (auto oc=m_OriginConnection.lock()) {
      if (auto fr=oc->getFileRegister()) {
        f = fr->getFile(index);
      }
    }

    if (f) {
      log::error(
        "cannot remove file {} from origin {}, not in list",
        f->debugName(), m_Name);
    } else {
      log::error(
        "cannot remove file {} from origin {}, "
        "not in list and not found in register",
        index, m_Name);
    }

    return;
  }

  m_Files.erase(itor);
}

std::shared_ptr<OriginConnection> FilesOrigin::getOriginConnection() const
{
  return m_OriginConnection.lock();
}

std::shared_ptr<FileRegister> FilesOrigin::getFileRegister() const
{
  if (auto oc=getOriginConnection()) {
    return oc->getFileRegister();
  }

  return {};
}

} //  namespace
