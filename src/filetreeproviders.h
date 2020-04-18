#ifndef MODORGANIZER_FILETREEPROVIDERS_INCLUDED
#define MODORGANIZER_FILETREEPROVIDERS_INCLUDED

#include "filetreefwd.h"
#include "fileregisterfwd.h"
#include <moassert.h>

class OrganizerCore;

namespace filetree
{

class Provider
{
  friend class Directory;
  friend class File;
  friend class DirectoryIterator;

public:
  Provider(const Provider&) = delete;
  Provider& operator=(const Provider&) = delete;
  virtual ~Provider() = default;

  virtual Directory root() = 0;
  virtual Directory findDirectoryRecursive(const std::wstring& path) = 0;

  virtual Directory childDirectoryAt(const Directory& d, std::size_t i) = 0;
  virtual std::size_t childDirectoryCount(const Directory& d) = 0;

  virtual File childFileAt(const Directory& d, std::size_t i) = 0;
  virtual std::size_t childFileCount(const Directory& d) = 0;

  virtual FileIndex childFileIndexAt(const Directory& d, std::size_t i) = 0;
  virtual std::size_t childFileIndexCount(const Directory& d) = 0;

protected:
  Provider() = default;

  virtual std::wstring_view name(const Directory& d) = 0;
  virtual bool topLevel(const Directory& d) = 0;
  virtual bool hasChildren(const Directory& d) = 0;

  virtual Directory findDirectoryImmediate(
    const Directory& d, lowercase_wstring_view path) = 0;

  virtual File findFileImmediate(
    const Directory& d, const MOShared::WStringViewKey& key) = 0;

  virtual File fileByIndex(const Directory& d, FileIndex index) = 0;


  virtual std::wstring_view name(const File& file) = 0;
  virtual fs::path path(const File& file) = 0;
  virtual std::optional<uint64_t> size(const File& file) = 0;
  virtual std::optional<uint64_t> compressedSize(const File& file) = 0;

  virtual FileIndex index(const File& file) = 0;
  virtual int originID(const File& file) = 0;
  virtual bool fromArchive(const File& file) = 0;
  virtual bool isConflicted(const File& file) = 0;
  virtual std::wstring_view archive(const File& file) = 0;
};



template <class Iterator>
class Container
{
public:
  Container(Provider& p, void* data)
    : m_provider(&p), m_data(data)
  {
  }

  Iterator begin()
  {
    MO_ASSERT(m_provider);
    return Iterator(*m_provider, m_data, 0);
  }

  Iterator end()
  {
    MO_ASSERT(m_provider);
    return Iterator(*m_provider, m_data, -1);
  }

private:
  Provider* m_provider;
  void* m_data;
};


using CountMF = std::size_t (Provider::*)(const Directory&);

template <class T>
using AtMF = T (Provider::*)(const Directory&, std::size_t);


template <class T, CountMF count, AtMF<T> at>
class Iterator
{
public:
  Iterator(Provider& p, void* data, std::size_t i);

  T operator*();
  Iterator& operator++();

  bool operator==(const Iterator& itor) const;
  bool operator!=(const Iterator& itor) const;

private:
  Provider* m_provider;
  void* m_data;
  std::size_t m_i;
};


using DirectoryContainer = Container<Iterator<
  Directory, &Provider::childDirectoryCount, &Provider::childDirectoryAt>>;

using FileContainer = Container<Iterator<
  File, &Provider::childFileCount, &Provider::childFileAt>>;

using FileIndexContainer = Container<Iterator<
  FileIndex, &Provider::childFileIndexCount, &Provider::childFileIndexAt>>;


class Directory
{
public:
  Directory(Provider& p, void* data);
  static Directory bad();

  std::wstring_view name() const;
  bool topLevel() const;
  bool hasChildren() const;

  Directory findDirectoryImmediate(lowercase_wstring_view path) const;
  File findFileImmediate(const MOShared::WStringViewKey& key) const;
  DirectoryContainer getImmediateDirectories() const;
  FileContainer getImmediateFiles() const;
  FileIndexContainer getImmediateFileIndices() const;
  File fileByIndex(FileIndex index) const;

  explicit operator bool() const;
  void* data() const;

private:
  Provider* m_provider;
  void* m_data;

  Directory();
};


class File
{
public:
  File(Provider& p, void* data);
  static File bad();

  std::wstring_view name() const;
  fs::path path() const;
  std::optional<uint64_t> size() const;
  std::optional<uint64_t> compressedSize() const;

  FileIndex index() const;
  int originID() const;
  bool fromArchive() const;
  bool isConflicted() const;
  std::wstring_view archive() const;

  explicit operator bool() const;
  void* data() const;

private:
  Provider* m_provider;
  void* m_data;

  File();
};



template <class DirectoryData, class FileData>
class BasicProvider : public Provider
{
protected:
  DirectoryData* data(const Directory& d) const
  {
    MO_ASSERT(d);
    MO_ASSERT(d.data());
    return static_cast<DirectoryData*>(d.data());
  }

  FileData* data(const File& f) const
  {
    MO_ASSERT(f);
    MO_ASSERT(f.data());
    return static_cast<FileData*>(f.data());
  }
};


class VirtualProvider : public BasicProvider<
  MOShared::DirectoryEntry, MOShared::FileEntry>
{
public:
  explicit VirtualProvider(OrganizerCore& core);

  Directory root() override;
  Directory findDirectoryRecursive(const std::wstring& path) override;

  Directory childDirectoryAt(const Directory& d, std::size_t i) override;
  std::size_t childDirectoryCount(const Directory& d) override;

  File childFileAt(const Directory& d, std::size_t i) override;
  std::size_t childFileCount(const Directory& d) override;

  FileIndex childFileIndexAt(const Directory& d, std::size_t i) override;
  std::size_t childFileIndexCount(const Directory& d) override;

private:
  OrganizerCore& m_core;

  std::wstring_view name(const Directory& d) override;
  bool topLevel(const Directory& d) override;
  bool hasChildren(const Directory& d) override;

  Directory findDirectoryImmediate(
    const Directory& d, lowercase_wstring_view path) override;

  File findFileImmediate(
    const Directory& d, const MOShared::WStringViewKey& key) override;

  File fileByIndex(const Directory& d, FileIndex index) override;


  std::wstring_view name(const File& file) override;
  fs::path path(const File& file) override;
  std::optional<uint64_t> size(const File& file) override;
  std::optional<uint64_t> compressedSize(const File& file) override;

  FileIndex index(const File& file) override;
  int originID(const File& file) override;
  bool fromArchive(const File& file) override;
  bool isConflicted(const File& file) override;
  std::wstring_view archive(const File& file) override;
};


struct FSFile
{
  fs::path path;
  std::wstring name;

  FSFile(fs::path p)
    : path(std::move(p)), name(path.filename().wstring())
  {
  }
};

struct FSDirectory
{
  fs::path path;

  std::wstring name;
  std::vector<std::unique_ptr<FSDirectory>> dirs;
  std::vector<std::unique_ptr<FSFile>> files;
  bool loaded;

  FSDirectory(fs::path p={})
    : path(std::move(p)), name(path.filename().wstring()), loaded(false)
  {
  }
};


class FilesystemProvider : public BasicProvider<FSDirectory, FSFile>
{
public:
  FilesystemProvider();
  FilesystemProvider(fs::path root, int originID);

  void setRoot(const fs::path& path, int originID);

  Directory root() override;
  Directory findDirectoryRecursive(const std::wstring& path) override;

  Directory childDirectoryAt(const Directory& d, std::size_t i) override;
  std::size_t childDirectoryCount(const Directory& d) override;

  File childFileAt(const Directory& d, std::size_t i) override;
  std::size_t childFileCount(const Directory& d) override;

  FileIndex childFileIndexAt(const Directory& d, std::size_t i) override;
  std::size_t childFileIndexCount(const Directory& d) override;

private:
  FSDirectory m_root;
  int m_origin;

  void load(FSDirectory& d);


  std::wstring_view name(const Directory& d) override;
  bool topLevel(const Directory& d) override;
  bool hasChildren(const Directory& d) override;

  Directory findDirectoryImmediate(
    const Directory& d, lowercase_wstring_view path) override;

  File findFileImmediate(
    const Directory& d, const MOShared::WStringViewKey& key) override;

  File fileByIndex(const Directory& d, FileIndex index) override;


  std::wstring_view name(const File& file) override;
  fs::path path(const File& file) override;
  std::optional<uint64_t> size(const File& file) override;
  std::optional<uint64_t> compressedSize(const File& file) override;

  FileIndex index(const File& file) override;
  int originID(const File& file) override;
  bool fromArchive(const File& file) override;
  bool isConflicted(const File& file) override;
  std::wstring_view archive(const File& file) override;
};


template <class T, CountMF count, AtMF<T> at>
Iterator<T, count, at>::Iterator(Provider& p, void* data, std::size_t i)
  : m_provider(&p), m_data(data), m_i(i)
{
  if (m_i == -1) {
    m_i = (m_provider->*count)(Directory(*m_provider, m_data));
  }
}

template <class T, CountMF count, AtMF<T> at>
T Iterator<T, count, at>::operator*()
{
  MO_ASSERT(m_provider);
  return (m_provider->*at)(Directory(*m_provider, m_data), m_i);
}

template <class T, CountMF count, AtMF<T> at>
Iterator<T, count, at>& Iterator<T, count, at>::operator++()
{
  MO_ASSERT(m_provider);
  MO_ASSERT((m_i + 1) <= (m_provider->*count)(Directory(*m_provider, m_data)));

  ++m_i;
  return *this;
}

template <class T, CountMF count, AtMF<T> at>
bool Iterator<T, count, at>::operator==(const Iterator& itor) const
{
  MO_ASSERT(m_provider == itor.m_provider);
  MO_ASSERT(m_data == itor.m_data);
  return (m_i == itor.m_i);
}

template <class T, CountMF count, AtMF<T> at>
bool Iterator<T, count, at>::operator!=(const Iterator& itor) const
{
  return !(*this == itor);
}

} // namespace

#endif // MODORGANIZER_FILETREEPROVIDERS_INCLUDED
