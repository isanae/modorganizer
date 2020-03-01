#include "filesorigin.h"
#include "originconnection.h"
#include "fileregister.h"
#include "fileentry.h"
#include <log.h>

namespace MOShared
{

using namespace MOBase;

FilesOrigin::FilesOrigin(
  OriginID ID, const OriginData& data, std::shared_ptr<OriginConnection> oc) :
    m_id(ID), m_enabled(true), m_name(data.name), m_path(data.path),
    m_priority(data.priority), m_originConnection(oc)
{
}

void FilesOrigin::setPriority(int priority)
{
  if (priority < 0) {
    log::error(
      "cannot set priority to {} for origin {}",
      priority, debugName());

    return;
  }

  m_priority = priority;
}

void FilesOrigin::setName(std::wstring_view name)
{
  if (name.empty()) {
    log::error(
      "cannot change origin name for {} to an empty string",
      debugName());

    return;
  }

  if (auto oc=m_originConnection.lock()) {
    oc->changeNameLookupInternal(m_name, name);
  }

  // the path should always match the name
  if (m_path.filename().native() != m_name) {
    log::warn(
      "files origin '{}': path '{}' doesn't end with name",
      m_name, m_path.native());
  }

  m_path = m_path.parent_path() / name;
  m_name = name;
}

std::vector<FileEntryPtr> FilesOrigin::getFiles() const
{
  std::vector<FileEntryPtr> v;

  auto oc = m_originConnection.lock();
  if (!oc) {
    return v;
  }

  auto fr = oc->getFileRegister();
  if (!fr) {
    return v;
  }

  {
    std::scoped_lock lock(m_filesMutex);

    for (const FileIndex& index : m_files) {
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
    std::scoped_lock lock(m_filesMutex);
    files = std::move(m_files);
    m_files.clear();
  }

  // removing files
  if (auto fr=getFileRegister()) {
    fr->removeOrigin(files, m_id);
  }

  m_enabled = false;
}

void FilesOrigin::setEnabledFlag()
{
  m_enabled = true;
}

void FilesOrigin::addFileInternal(FileIndex index)
{
  std::scoped_lock lock(m_filesMutex);
  m_files.insert(index);
}

void FilesOrigin::removeFileInternal(FileIndex index)
{
  std::scoped_lock lock(m_filesMutex);

  auto itor = m_files.find(index);

  if (itor == m_files.end()) {
    // just logging

    FileEntryPtr f;

    if (auto oc=m_originConnection.lock()) {
      if (auto fr=oc->getFileRegister()) {
        f = fr->getFile(index);
      }
    }

    if (f) {
      log::error(
        "cannot remove file {} from origin {}, not in list",
        f->debugName(), m_name);
    } else {
      log::error(
        "cannot remove file {} from origin {}, "
        "not in list and not found in register",
        index, m_name);
    }

    return;
  }

  m_files.erase(itor);
}

std::shared_ptr<OriginConnection> FilesOrigin::getOriginConnection() const
{
  return m_originConnection.lock();
}

std::shared_ptr<FileRegister> FilesOrigin::getFileRegister() const
{
  if (auto oc=getOriginConnection()) {
    return oc->getFileRegister();
  }

  return {};
}

std::wstring FilesOrigin::debugName() const
{
  return fmt::format(L"{}:{}", m_name, m_id);
}

} //  namespace
