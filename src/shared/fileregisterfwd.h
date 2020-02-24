#ifndef MO_REGISTER_FILEREGISTERFWD_INCLUDED
#define MO_REGISTER_FILEREGISTERFWD_INCLUDED


//                         +--------------------+
//                 +------ | DirectoryStructure | --------------+
//                 |       +--------------------+               |
//                 v                                            |
//          +--------------+                                    |
//          | FileRegister | <-------------(ref)-------------+  |
//          +--------------+                                 |  |
//            ^          \                                   |  |
//           /            \                                  |  |
//          v              v                                 ^  v
// +------------------+  +-----------+               +-----------------------+
// | OriginConnection |  | FileEntry | ---(ref)----> | DirectoryEntry (root) |
// +------------------+  +-----------+ <-(index)---- +-----------------------+
//       ^                    ^                                ^  v
//       |                    |                                |  | (children)
//       v                    |                                +--+
// +-------------+            |
// | FilesOrigin |---(index)--+
// +-------------+


class DirectoryRefreshProgress;
class DirectoryStructure;

namespace MOShared
{

struct WStringKey;

struct WStringViewKey
{
  explicit WStringViewKey(std::wstring_view v)
    : value(v), hash(getHash(value))
  {
  }

  inline WStringViewKey(const WStringKey& k);

  bool operator==(const WStringViewKey& o) const
  {
    return (value == o.value);
  }

  static std::size_t getHash(std::wstring_view value)
  {
    return std::hash<std::wstring_view>()(value);
  }

  std::wstring_view value;
  const std::size_t hash;
};


struct WStringKey
{
  explicit WStringKey(std::wstring v)
    : value(std::move(v)), hash(getHash(value))
  {
  }

  bool operator==(const WStringViewKey& o) const
  {
    return (value == o.value);
  }

  bool operator==(const WStringKey& o) const
  {
    return (value == o.value);
  }

  static std::size_t getHash(const std::wstring& value)
  {
    return std::hash<std::wstring>()(value);
  }

  std::wstring value;
  const std::size_t hash;
};


WStringViewKey::WStringViewKey(const WStringKey& k)
  : value(k.value), hash(k.hash)
{
}


class DirectoryEntry;
class OriginConnection;
class FileRegister;
class FilesOrigin;
class FileEntry;
struct DirectoryStats;

using FileEntryPtr = std::shared_ptr<FileEntry>;
using FileIndex = unsigned int;
using FileKey = WStringKey;
using FileKeyView = WStringViewKey;

using OriginID = int;

constexpr FileIndex InvalidFileIndex = UINT_MAX;
constexpr OriginID InvalidOriginID = -1;
constexpr OriginID DataOriginID = 0;

// the filename of an archive and the load order of its associated plugin
//
struct ArchiveInfo
{
  std::wstring name;
  int order;

  ArchiveInfo()
    : order(-1)
  {
  }

  ArchiveInfo(std::wstring name, int order)
    : name(std::move(name)), order(order)
  {
  }
};

// a mod id and an archive, used by FileEntry to remember alternative origins
//
struct OriginInfo
{
  OriginID originID;
  ArchiveInfo archive;

  OriginInfo()
    : originID(InvalidOriginID)
  {
  }

  OriginInfo(OriginID id, ArchiveInfo a)
    : originID(id), archive(std::move(a))
  {
  }

  // returns a string that represents this file, such as "originid:archive";
  // useful for logging
  //
  std::wstring debugName() const;
};


struct DirectoryStats
{
  static constexpr bool EnableInstrumentation = false;

  std::string mod;

  std::chrono::nanoseconds dirTimes;
  std::chrono::nanoseconds fileTimes;
  std::chrono::nanoseconds sortTimes;

  std::chrono::nanoseconds subdirLookupTimes;
  std::chrono::nanoseconds addDirectoryTimes;

  std::chrono::nanoseconds filesLookupTimes;
  std::chrono::nanoseconds addFileTimes;
  std::chrono::nanoseconds addOriginToFileTimes;
  std::chrono::nanoseconds addFileToOriginTimes;
  std::chrono::nanoseconds addFileToRegisterTimes;

  DirectoryStats();

  DirectoryStats& operator+=(const DirectoryStats& o);

  static std::string csvHeader();
  std::string toCsv() const;
};

} // namespace


namespace std
{

template <>
struct hash<MOShared::WStringKey>
{
  using argument_type = MOShared::WStringKey;
  using result_type = std::size_t;
  using is_transparent = void;

  inline result_type operator()(const MOShared::WStringKey& key) const
  {
    return key.hash;
  }

  inline result_type operator()(const MOShared::WStringViewKey& key) const
  {
    return key.hash;
  }
};

template <>
struct hash<MOShared::WStringViewKey>
{
  using argument_type = MOShared::WStringViewKey;
  using result_type = std::size_t;
  using is_transparent = void;

  inline result_type operator()(const argument_type& key) const
  {
    return key.hash;
  }
};

} // namespace

#endif // MO_REGISTER_FILEREGISTERFWD_INCLUDED
