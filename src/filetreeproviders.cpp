#include "filetreeproviders.h"
#include "organizercore.h"
#include "directoryentry.h"
#include "fileentry.h"

namespace filetree
{

using namespace MOShared;

Directory::Directory()
  : m_provider(nullptr), m_data(nullptr)
{
}

Directory::Directory(Provider& p, void* data)
  : m_provider(&p), m_data(data)
{
}

Directory Directory::bad()
{
  return {};
}

std::wstring_view Directory::name() const
{
  MO_ASSERT(m_provider);
  return m_provider->name(*this);
}

bool Directory::topLevel() const
{
  MO_ASSERT(m_provider);
  return m_provider->topLevel(*this);
}

bool Directory::hasChildren() const
{
  MO_ASSERT(m_provider);
  return m_provider->hasChildren(*this);
}

Directory Directory::findDirectoryImmediate(lowercase_wstring_view path) const
{
  MO_ASSERT(m_provider);
  return m_provider->findDirectoryImmediate(*this, path);
}

File Directory::findFileImmediate(const WStringViewKey& key) const
{
  MO_ASSERT(m_provider);
  return m_provider->findFileImmediate(*this, key);
}

DirectoryContainer Directory::getImmediateDirectories() const
{
  MO_ASSERT(m_provider);
  return DirectoryContainer(*m_provider, m_data);
}

FileContainer Directory::getImmediateFiles() const
{
  MO_ASSERT(m_provider);
  return FileContainer(*m_provider, m_data);
}

FileIndexContainer Directory::getImmediateFileIndices() const
{
  MO_ASSERT(m_provider);
  return FileIndexContainer(*m_provider, m_data);
}

File Directory::fileByIndex(FileIndex index) const
{
  MO_ASSERT(m_provider);
  return m_provider->fileByIndex(*this, index);
}

Directory::operator bool() const
{
  return (m_provider != nullptr);
}

void* Directory::data() const
{
  return m_data;
}


File::File()
  : m_provider(nullptr), m_data(nullptr)
{
}

File::File(Provider& p, void* data)
  : m_provider(&p), m_data(data)
{
}

File File::bad()
{
  return {};
}

std::wstring_view File::name() const
{
  MO_ASSERT(m_provider);
  return m_provider->name(*this);
}

fs::path File::path() const
{
  MO_ASSERT(m_provider);
  return m_provider->path(*this);
}

std::optional<uint64_t> File::size() const
{
  MO_ASSERT(m_provider);
  return m_provider->size(*this);
}

std::optional<uint64_t> File::compressedSize() const
{
  MO_ASSERT(m_provider);
  return m_provider->compressedSize(*this);
}

FileIndex File::index() const
{
  MO_ASSERT(m_provider);
  return m_provider->index(*this);
}

int File::originID() const
{
  MO_ASSERT(m_provider);
  return m_provider->originID(*this);
}

bool File::fromArchive() const
{
  MO_ASSERT(m_provider);
  return m_provider->fromArchive(*this);
}

bool File::isConflicted() const
{
  MO_ASSERT(m_provider);
  return m_provider->isConflicted(*this);
}

std::wstring_view File::archive() const
{
  MO_ASSERT(m_provider);
  return m_provider->archive(*this);
}

File::operator bool() const
{
  return (m_provider != nullptr);
}

void* File::data() const
{
  return m_data;
}



VirtualProvider::VirtualProvider(OrganizerCore& core)
  : m_core(core)
{
}

Directory VirtualProvider::root()
{
  return Directory(*this, m_core.directoryStructure());
}

Directory VirtualProvider::findDirectoryRecursive(const std::wstring& path)
{
  auto* parentEntry = m_core.directoryStructure()
    ->findSubDirectoryRecursive(path);

  if (parentEntry)
    return Directory(*this, parentEntry);
  else
    return Directory::bad();
}


std::wstring_view VirtualProvider::name(const Directory& d)
{
  MO_ASSERT(d);
  auto* e = static_cast<DirectoryEntry*>(d.data());

  return e->getName();
}

bool VirtualProvider::topLevel(const Directory& d)
{
  MO_ASSERT(d);
  auto* e = static_cast<DirectoryEntry*>(d.data());

  return e->isTopLevel();
}

bool VirtualProvider::hasChildren(const Directory& d)
{
  MO_ASSERT(d);
  auto* e = static_cast<DirectoryEntry*>(d.data());

  return !e->isEmpty();
}

Directory VirtualProvider::findDirectoryImmediate(
  const Directory& d, lowercase_wstring_view path)
{
  MO_ASSERT(d);
  auto* e = static_cast<DirectoryEntry*>(d.data());

  auto* sd = e->findSubDirectory(path, true);

  if (sd) {
    return Directory(*this, sd);
  } else {
    return Directory::bad();
  }
}

File VirtualProvider::findFileImmediate(
  const Directory& d, const WStringViewKey& key)
{
  MO_ASSERT(d);
  auto* e = static_cast<DirectoryEntry*>(d.data());

  auto f = e->findFile(key);

  if (f) {
    return File(*this, f.get());
  } else {
    return File::bad();
  }
}

File VirtualProvider::fileByIndex(const Directory& d, FileIndex index)
{
  MO_ASSERT(d);
  auto* e = static_cast<DirectoryEntry*>(d.data());

  auto f = e->getFileByIndex(index);

  if (f) {
    return File(*this, f.get());
  } else {
    return File::bad();
  }
}


Directory VirtualProvider::childDirectoryAt(const Directory& d, std::size_t i)
{
  MO_ASSERT(d);
  auto* e = static_cast<DirectoryEntry*>(d.data());

  MO_ASSERT(i < e->getSubDirectories().size());
  return Directory(*this, e->getSubDirectories()[i]);
}

std::size_t VirtualProvider::childDirectoryCount(const Directory& d)
{
  MO_ASSERT(d);
  auto* e = static_cast<DirectoryEntry*>(d.data());

  return e->getSubDirectories().size();
}

File VirtualProvider::childFileAt(const Directory& d, std::size_t i)
{
  MO_ASSERT(d);
  auto* e = static_cast<DirectoryEntry*>(d.data());

  MO_ASSERT(i < e->getFileMap().size());

  const auto& map = e->getFileMap();
  auto itor = map.begin();
  std::advance(itor, i);

  auto f = e->getFileByIndex(itor->second);

  if (f) {
    return File(*this, f.get());
  } else {
    return File::bad();
  }
}

std::size_t VirtualProvider::childFileCount(const Directory& d)
{
  MO_ASSERT(d);
  auto* e = static_cast<DirectoryEntry*>(d.data());

  return e->getFiles().size();
}

FileIndex VirtualProvider::childFileIndexAt(const Directory& d, std::size_t i)
{
  MO_ASSERT(d);
  auto* e = static_cast<DirectoryEntry*>(d.data());

  MO_ASSERT(i < e->getFileMap().size());
  const auto& map = e->getFileMap();
  auto itor = map.begin();
  std::advance(itor, i);

  return itor->second;
}

std::size_t VirtualProvider::childFileIndexCount(const Directory& d)
{
  MO_ASSERT(d);
  auto* e = static_cast<DirectoryEntry*>(d.data());

  return e->getFiles().size();
}


std::wstring_view VirtualProvider::name(const File& f)
{
  MO_ASSERT(f);
  auto* e = static_cast<FileEntry*>(f.data());

  return e->getName();
}

fs::path VirtualProvider::path(const File& f)
{
  MO_ASSERT(f);
  auto* e = static_cast<FileEntry*>(f.data());

  return e->getFullPath();
}

std::optional<uint64_t> VirtualProvider::size(const File& f)
{
  MO_ASSERT(f);
  auto* e = static_cast<FileEntry*>(f.data());

  const auto s = e->getFileSize();

  if (s == FileEntry::NoFileSize) {
    return {};
  } else {
    return s;
  }
}

std::optional<uint64_t> VirtualProvider::compressedSize(const File& f)
{
  MO_ASSERT(f);
  auto* e = static_cast<FileEntry*>(f.data());

  const auto s = e->getCompressedFileSize();

  if (s == FileEntry::NoFileSize) {
    return {};
  } else {
    return s;
  }
}

FileIndex VirtualProvider::index(const File& f)
{
  MO_ASSERT(f);
  auto* e = static_cast<FileEntry*>(f.data());

  return e->getIndex();
}

int VirtualProvider::originID(const File& f)
{
  MO_ASSERT(f);
  auto* e = static_cast<FileEntry*>(f.data());

  return e->getOrigin();
}

bool VirtualProvider::fromArchive(const File& f)
{
  MO_ASSERT(f);
  auto* e = static_cast<FileEntry*>(f.data());

  return e->isFromArchive();
}

bool VirtualProvider::isConflicted(const File& f)
{
  MO_ASSERT(f);
  auto* e = static_cast<FileEntry*>(f.data());

  return !e->getAlternatives().empty();
}

std::wstring_view VirtualProvider::archive(const File& f)
{
  MO_ASSERT(f);
  auto* e = static_cast<FileEntry*>(f.data());

  return e->getArchive().first;
}

} // namespace
