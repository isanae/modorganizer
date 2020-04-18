#include "filetreeproviders.h"
#include "organizercore.h"
#include "directoryentry.h"
#include "fileentry.h"
#include "envfs.h"
#include <util.h>

namespace filetree
{

using namespace MOShared;
using namespace MOBase;

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
  return data(d)->getName();
}

bool VirtualProvider::topLevel(const Directory& d)
{
  return data(d)->isTopLevel();
}

bool VirtualProvider::hasChildren(const Directory& d)
{
  return !data(d)->isEmpty();
}

Directory VirtualProvider::findDirectoryImmediate(
  const Directory& d, lowercase_wstring_view path)
{
  auto* sd = data(d)->findSubDirectory(path, true);

  if (sd) {
    return Directory(*this, sd);
  } else {
    return Directory::bad();
  }
}

File VirtualProvider::findFileImmediate(
  const Directory& d, const WStringViewKey& key)
{
  auto f = data(d)->findFile(key);

  if (f) {
    return File(*this, f.get());
  } else {
    return File::bad();
  }
}

File VirtualProvider::fileByIndex(const Directory& d, FileIndex index)
{
  auto f = data(d)->getFileByIndex(static_cast<unsigned int>(index));

  if (f) {
    return File(*this, f.get());
  } else {
    return File::bad();
  }
}


Directory VirtualProvider::childDirectoryAt(const Directory& d, std::size_t i)
{
  MO_ASSERT(i < data(d)->getSubDirectories().size());
  return Directory(*this, data(d)->getSubDirectories()[i]);
}

std::size_t VirtualProvider::childDirectoryCount(const Directory& d)
{
  return data(d)->getSubDirectories().size();
}

File VirtualProvider::childFileAt(const Directory& d, std::size_t i)
{
  auto* e = data(d);

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
  return data(d)->getFiles().size();
}

FileIndex VirtualProvider::childFileIndexAt(const Directory& d, std::size_t i)
{
  auto* e = data(d);

  MO_ASSERT(i < e->getFileMap().size());
  const auto& map = e->getFileMap();
  auto itor = map.begin();
  std::advance(itor, i);

  return itor->second;
}

std::size_t VirtualProvider::childFileIndexCount(const Directory& d)
{
  return data(d)->getFiles().size();
}


std::wstring_view VirtualProvider::name(const File& f)
{
  return data(f)->getName();
}

fs::path VirtualProvider::path(const File& f)
{
  return data(f)->getFullPath();
}

std::optional<uint64_t> VirtualProvider::size(const File& f)
{
  const auto s = data(f)->getFileSize();

  if (s == FileEntry::NoFileSize) {
    return {};
  } else {
    return s;
  }
}

std::optional<uint64_t> VirtualProvider::compressedSize(const File& f)
{
  const auto s = data(f)->getCompressedFileSize();

  if (s == FileEntry::NoFileSize) {
    return {};
  } else {
    return s;
  }
}

FileIndex VirtualProvider::index(const File& f)
{
  return data(f)->getIndex();
}

int VirtualProvider::originID(const File& f)
{
  return data(f)->getOrigin();
}

bool VirtualProvider::fromArchive(const File& f)
{
  return data(f)->isFromArchive();
}

bool VirtualProvider::isConflicted(const File& f)
{
  return !data(f)->getAlternatives().empty();
}

std::wstring_view VirtualProvider::archive(const File& f)
{
  return data(f)->getArchive().first;
}


FilesystemProvider::FilesystemProvider()
  : m_origin(InvalidOriginID)
{
}

FilesystemProvider::FilesystemProvider(fs::path root, int originID)
  : FilesystemProvider()
{
  setRoot(root, originID);
  load(m_root);
}

void FilesystemProvider::setRoot(const fs::path& path, int originID)
{
  auto c = fs::canonical(path);
  m_root = FSDirectory(c);
  m_origin = originID;
}

Directory FilesystemProvider::root()
{
  return Directory(*this, &m_root);
}

Directory FilesystemProvider::findDirectoryRecursive(const std::wstring& path)
{
  std::vector<std::wstring_view> cs;

  std::size_t start = 0;
  for (;;) {
    std::size_t sep = path.find_first_of(L"\\/", start);

    if (sep == std::wstring::npos) {
      if (start < path.size()) {
        cs.push_back({path.c_str() + start, path.size() - start});
      }

      break;
    } else {
      if (sep - start > 0) {
        cs.push_back({path.c_str() + start, sep - start});
      }

      start = sep + 1;
    }
  }

  FSDirectory* d = &m_root;

  for (auto&& name : cs) {
    load(*d);

    bool found = false;

    for (auto&& sd : d->dirs) {
      if (ToLowerCopy(sd->name) == ToLowerCopy(name)) {
        d = sd.get();
        found = true;
        break;
      }
    }

    if (!found)
      return Directory::bad();
  }

  return Directory(*this, d);
}


std::wstring_view FilesystemProvider::name(const Directory& d)
{
  return data(d)->name;
}

bool FilesystemProvider::topLevel(const Directory& d)
{
  return (data(d) == &m_root);
}

bool FilesystemProvider::hasChildren(const Directory& d)
{
  auto* e = data(d);

  load(*e);
  return (!e->dirs.empty() || !e->files.empty());
}

Directory FilesystemProvider::findDirectoryImmediate(
  const Directory& d, lowercase_wstring_view path)
{
  auto* e = data(d);

  load(*e);

  for (auto&& d : e->dirs) {
    if (ToLowerCopy(d->name) == path) {
      return Directory(*this, &d);
    }
  }

  return Directory::bad();
}

File FilesystemProvider::findFileImmediate(
  const Directory& d, const WStringViewKey& key)
{
  auto* e = data(d);

  load(*e);

  for (auto&& f : e->files) {
    if (ToLowerCopy(f->name) == key.value) {
      return File(*this, f.get());
    }
  }

  return File::bad();
}

File FilesystemProvider::fileByIndex(const Directory& d, FileIndex index)
{
  auto* e = data(d);

  load(*e);

  for (auto&& f : e->files) {
    if (std::hash<std::wstring>()(f->path.native()) == index) {
      return File(*this, f.get());
    }
  }

  return File::bad();
}


Directory FilesystemProvider::childDirectoryAt(const Directory& d, std::size_t i)
{
  auto* e = data(d);

  load(*e);

  MO_ASSERT(i < e->dirs.size());
  return Directory(*this, e->dirs[i].get());
}

std::size_t FilesystemProvider::childDirectoryCount(const Directory& d)
{
  auto* e = data(d);

  load(*e);
  return e->dirs.size();
}

File FilesystemProvider::childFileAt(const Directory& d, std::size_t i)
{
  auto* e = data(d);

  load(*e);

  MO_ASSERT(i < e->files.size());
  return File(*this, e->files[i].get());
}

std::size_t FilesystemProvider::childFileCount(const Directory& d)
{
  auto* e = data(d);

  load(*e);

  return e->files.size();
}

FileIndex FilesystemProvider::childFileIndexAt(const Directory& d, std::size_t i)
{
  auto* e = data(d);

  load(*e);

  MO_ASSERT(i < e->files.size());
  return std::hash<std::wstring>()(e->files[i]->path.native());
}

std::size_t FilesystemProvider::childFileIndexCount(const Directory& d)
{
  auto* e = data(d);

  load(*e);

  return e->files.size();
}


void FilesystemProvider::load(FSDirectory& d)
{
  if (d.loaded) {
    return;
  }

  d.loaded = true;

  if (d.path.empty()) {
    return;
  }

  const std::wstring what = d.path.native() + L"\\*";
  WIN32_FIND_DATAW fd = {};

  HANDLE h = ::FindFirstFileW(what.c_str(), &fd);

  if (h == INVALID_HANDLE_VALUE) {
    const auto e = GetLastError();
    log::error("can't load {}: {}", what, formatSystemMessage(e));
    return;
  }

  for (;;) {
    std::wstring name = fd.cFileName;

    if (name != L"." && name != L"..") {
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        d.dirs.push_back(std::make_unique<FSDirectory>(d.path / fd.cFileName));
      } else {
        d.files.push_back(std::make_unique<FSFile>(d.path / fd.cFileName));
      }
    }

    if (!FindNextFileW(h, &fd)) {
      const auto e = GetLastError();

      if (e != ERROR_NO_MORE_FILES) {
        log::error("can't find next file in {}: {}", what, formatSystemMessage(e));
      }

      break;
    }
  }

  FindClose(h);
}


std::wstring_view FilesystemProvider::name(const File& f)
{
  return data(f)->name;
}

fs::path FilesystemProvider::path(const File& f)
{
  return data(f)->path;
}

std::optional<uint64_t> FilesystemProvider::size(const File& f)
{
  auto* e = data(f);

  std::error_code ec;
  const auto s = fs::file_size(e->path, ec);

  if (ec) {
    log::error(
      "failed to get file size of {}: {}",
      e->path.string(), ec.message());

    return {};
  }

  return s;
}

std::optional<uint64_t> FilesystemProvider::compressedSize(const File&)
{
  return {};
}

FileIndex FilesystemProvider::index(const File& f)
{
  return std::hash<std::wstring>()(data(f)->path.native());
}

int FilesystemProvider::originID(const File&)
{
  return m_origin;
}

bool FilesystemProvider::fromArchive(const File&)
{
  return false;
}

bool FilesystemProvider::isConflicted(const File&)
{
  return false;
}

std::wstring_view FilesystemProvider::archive(const File&)
{
  return {};
}

} // namespace
