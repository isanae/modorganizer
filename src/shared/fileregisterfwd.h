#ifndef MO_REGISTER_FILEREGISTERFWD_INCLUDED
#define MO_REGISTER_FILEREGISTERFWD_INCLUDED

#include <filesystem>

//                         +--------------------+
//                 +------ | DirectoryStructure | -------------+
//                 |       +--------------------+              |
//                 v                                           |
//          +--------------+                                   |
//          | FileRegister | <-------------(ref)------------+  |
//          +--------------+                                |  |
//            ^          \                                  |  |
//           /            \                                 |  |
//          v              v                                ^  v
// +------------------+  +-----------+  --(ref)--->  +----------------+
// | OriginConnection |  | FileEntry |               | DirectoryEntry |
// +------------------+  +-----------+  <-(index)--  +----------------+
//       ^                    ^  v                          ^  v
//       |                    |  |                          |  | (children)
//       v                    |  |                          +--+
// +-------------+  >-(index)-+  |
// | FilesOrigin |               |
// +-------------+  <-(index)----+
//
//
// there is only one DirectoryStructure, owned by OrganizerCore; it has the
// FileRegister and root DirectoryEntry
//
// when refreshing the DirectoryStructure, it creates a new register and root
// directory and swaps them with the internal objects when finished
//
// the register owns all the FileEntry objects; the FilesOrigin and
// DirectoryEntry objects use FileIndex to refer to them
//
// there are lots of references between the various classes, mostly for faster
// lookups; for example, a FileEntry is:
//   1) owned by the FileRegister in a flat list,
//   2) referred to by index by any FilesOrigin that has the file, and
//   3) referred to by index by DirectoryEntry that contains the file,
// the FileEntry also refers to the FilesOrigin by its index
//
// most of these classes have multiple data structures that refer to the
// same data for faster lookups while keeping the order, like both a vector and
// a hash map
//
// which means a lot of work has to be done to maintain all these objects
// synchronized
//
// most entry points are in FileRegister, which takes care of keeping the
// various structures synchronized; for example, adding a new file to the
// structure requires adding the file to the directory, the file to the origin
// and the origin to the file
//
// therefore, member functions that are named somethingInternal() are not entry
// points: they are used by the FileRegister or other classes to change data,
// but they are not typically sufficient to update the whole structure and will
// create discrepancies between the various objects


class DirectoryStructure;
class DirectoryRefreshProgress;

namespace MOShared
{

// the two WStringViewKey and WStringKey classes are used by DirectoryEntry,
// they're keys in the hash maps
//
// FileTreeModel also uses them to do faster lookup instead of storing file
// names and having to do a bunch of string comparisons

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

using FileEntryPtr = std::shared_ptr<FileEntry>;
using FileIndex = unsigned int;
using FileKey = WStringKey;
using FileKeyView = WStringViewKey;

using OriginID = int;

constexpr FileIndex InvalidFileIndex = UINT_MAX;
constexpr OriginID InvalidOriginID = -1;
constexpr OriginID DataOriginID = 0;
constexpr int InvalidOrder = -1;

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

  bool operator==(const ArchiveInfo& other) const
  {
    return (name == other.name) && (order == other.order);
  }

  bool operator!=(const ArchiveInfo& other) const
  {
    return !(*this == other);
  }

  // returns a string that represents this file, such as "name:order";
  // useful for logging
  //
  std::wstring debugName() const;
  friend std::ostream& operator<<(std::ostream& out, const ArchiveInfo& a);
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

  bool operator==(const OriginInfo& other) const
  {
    return (originID == other.originID) && (archive == other.archive);
  }

  bool operator!=(const OriginInfo& other) const
  {
    return !(*this == other);
  }

  // returns a string that represents this file, such as "originid:archive";
  // useful for logging
  //
  std::wstring debugName() const;
  friend std::ostream& operator<<(std::ostream& out, const OriginInfo& a);
};


struct OriginData
{
  std::wstring_view name;
  std::filesystem::path path;
  int priority;
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
