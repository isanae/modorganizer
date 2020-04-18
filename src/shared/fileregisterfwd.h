#ifndef MO_REGISTER_FILEREGISTERFWD_INCLUDED
#define MO_REGISTER_FILEREGISTERFWD_INCLUDED

class DirectoryRefreshProgress;

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

using FileEntryPtr = boost::shared_ptr<FileEntry>;
using FileIndex = unsigned int;
using OriginID = int;

constexpr FileIndex InvalidFileIndex = UINT_MAX;
constexpr OriginID InvalidOriginID = -1;


// a vector of {originId, {archiveName, order}}
//
// if a file is in an archive, archiveName is the name of the bsa and order
// is the order of the associated plugin in the plugins list
//
// is a file is not in an archive, archiveName is empty and order is usually
// -1
using AlternativesVector = std::vector<std::pair<OriginID, std::pair<std::wstring, int>>>;

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

  int64_t originExists;
  int64_t originCreate;
  int64_t originsNeededEnabled;

  int64_t subdirExists;
  int64_t subdirCreate;

  int64_t fileExists;
  int64_t fileCreate;
  int64_t filesInsertedInRegister;
  int64_t filesAssignedInRegister;

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

} // namespace std

#endif // MO_REGISTER_FILEREGISTERFWD_INCLUDED
