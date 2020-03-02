#include "envfs.h"
#include "env.h"
#include "util.h"
#include <utility.h>
#include <log.h>

using namespace MOBase;

typedef struct _UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
  ULONG Length;
  HANDLE RootDirectory;
  PUNICODE_STRING ObjectName;
  ULONG Attributes;
  PVOID SecurityDescriptor;
  PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;


typedef struct _FILE_DIRECTORY_INFORMATION {
  ULONG         NextEntryOffset;
  ULONG         FileIndex;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER EndOfFile;
  LARGE_INTEGER AllocationSize;
  ULONG         FileAttributes;
  ULONG         FileNameLength;
  WCHAR         FileName[1];
} FILE_DIRECTORY_INFORMATION, *PFILE_DIRECTORY_INFORMATION;


#define FILE_SHARE_VALID_FLAGS            0x00000007

// copied from ntstatus.h
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_BUFFER_OVERFLOW ((NTSTATUS)0x80000005L)
#define STATUS_NO_MORE_FILES ((NTSTATUS)0x80000006L)
#define STATUS_NO_SUCH_FILE ((NTSTATUS)0xC000000FL)

typedef struct _IO_STATUS_BLOCK IO_STATUS_BLOCK;

typedef struct _IO_STATUS_BLOCK *PIO_STATUS_BLOCK;
// typedef VOID (NTAPI *PIO_APC_ROUTINE )(__in PVOID ApcContext, __in
// PIO_STATUS_BLOCK IoStatusBlock, __in ULONG Reserved);
typedef VOID(NTAPI *PIO_APC_ROUTINE)(PVOID ApcContext,
  PIO_STATUS_BLOCK IoStatusBlock,
  ULONG Reserved);


typedef enum _FILE_INFORMATION_CLASS {
  FileDirectoryInformation = 1
} FILE_INFORMATION_CLASS;

typedef NTSTATUS(WINAPI *NtQueryDirectoryFile_type)(
  HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID, PIO_STATUS_BLOCK, PVOID, ULONG,
  FILE_INFORMATION_CLASS, BOOLEAN, PUNICODE_STRING, BOOLEAN);

typedef NTSTATUS(WINAPI *NtOpenFile_type)(PHANDLE, ACCESS_MASK,
  POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK,
  ULONG, ULONG);

typedef NTSTATUS(WINAPI *NtClose_type)(HANDLE);


NtOpenFile_type NtOpenFile = nullptr;
NtQueryDirectoryFile_type NtQueryDirectoryFile = nullptr;
extern NtClose_type NtClose = nullptr;


#define FILE_DIRECTORY_FILE                     0x00000001
#define FILE_WRITE_THROUGH                      0x00000002
#define FILE_SEQUENTIAL_ONLY                    0x00000004
#define FILE_NO_INTERMEDIATE_BUFFERING          0x00000008

#define FILE_SYNCHRONOUS_IO_ALERT               0x00000010
#define FILE_SYNCHRONOUS_IO_NONALERT            0x00000020
#define FILE_NON_DIRECTORY_FILE                 0x00000040
#define FILE_CREATE_TREE_CONNECTION             0x00000080

#define FILE_COMPLETE_IF_OPLOCKED               0x00000100
#define FILE_NO_EA_KNOWLEDGE                    0x00000200
#define FILE_OPEN_REMOTE_INSTANCE               0x00000400
#define FILE_RANDOM_ACCESS                      0x00000800

#define FILE_DELETE_ON_CLOSE                    0x00001000
#define FILE_OPEN_BY_FILE_ID                    0x00002000
#define FILE_OPEN_FOR_BACKUP_INTENT             0x00004000
#define FILE_NO_COMPRESSION                     0x00008000

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN7)
#define FILE_OPEN_REQUIRING_OPLOCK              0x00010000
#endif

#define FILE_RESERVE_OPFILTER                   0x00100000
#define FILE_OPEN_REPARSE_POINT                 0x00200000
#define FILE_OPEN_NO_RECALL                     0x00400000
#define FILE_OPEN_FOR_FREE_SPACE_QUERY          0x00800000

#define FILE_VALID_OPTION_FLAGS                 0x00ffffff
#define FILE_VALID_PIPE_OPTION_FLAGS            0x00000032
#define FILE_VALID_MAILSLOT_OPTION_FLAGS        0x00000032
#define FILE_VALID_SET_FLAGS                    0x00000036

typedef struct _IO_STATUS_BLOCK {
#pragma warning(push)
#pragma warning(disable: 4201) // we'll always use the Microsoft compiler
  union {
    NTSTATUS Status;
    PVOID Pointer;
  } DUMMYUNIONNAME;
#pragma warning(pop)

  ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;



namespace env
{

std::wstring_view toStringView(const UNICODE_STRING* s)
{
  if (s && s->Buffer) {
    return {s->Buffer, (s->Length / sizeof(wchar_t))};
  } else {
    return {};
  }
}

std::wstring_view toStringView(POBJECT_ATTRIBUTES poa)
{
  if (poa->ObjectName) {
    return toStringView(poa->ObjectName);
  }

  return {};
}

QString toString(POBJECT_ATTRIBUTES poa)
{
  const auto sv = toStringView(poa);
  return QString::fromWCharArray(sv.data(), static_cast<int>(sv.size()));
}


class HandleCloserThread
{
public:
  HandleCloserThread()
    : m_ready(false)
  {
    m_handles.reserve(50000);
  }

  void shrink()
  {
    m_handles.shrink_to_fit();
  }

  void add(HANDLE h)
  {
    m_handles.push_back(h);
  }

  void wakeup()
  {
    {
      m_ready = true;
      std::unique_lock lock(m_mutex);
    }

    m_cv.notify_one();
  }

  void run()
  {
    SetThisThreadName("HandleCloserThread");

    std::unique_lock lock(m_mutex);
    m_cv.wait(lock, [&]{ return m_ready; });

    closeHandles();
  }

private:
  std::vector<HANDLE> m_handles;
  std::condition_variable m_cv;
  std::mutex m_mutex;
  bool m_ready;

  void closeHandles()
  {
    for (auto& h : m_handles) {
      NtClose(h);
    }

    m_handles.clear();
    m_ready = false;
  }
};

constexpr std::size_t AllocSize = 1024 * 1024;
static ThreadPool<HandleCloserThread> g_handleClosers;

void setHandleCloserThreadCount(std::size_t n)
{
  g_handleClosers.setMax(n);
}

void forEachEntryImpl(
  void* cx, HandleCloserThread& hc, std::vector<std::unique_ptr<unsigned char[]>>& buffers,
  POBJECT_ATTRIBUTES poa, std::size_t depth,
  DirStartF* dirStartF, DirEndF* dirEndF, FileF* fileF)
{
  IO_STATUS_BLOCK iosb;
  UNICODE_STRING ObjectName;
  OBJECT_ATTRIBUTES oa = { sizeof(oa), 0, &ObjectName };
  NTSTATUS status;

  status = NtOpenFile(
    &oa.RootDirectory, FILE_GENERIC_READ, poa, &iosb, FILE_SHARE_VALID_FLAGS,
    FILE_SYNCHRONOUS_IO_NONALERT|FILE_OPEN_REPARSE_POINT|FILE_OPEN_FOR_BACKUP_INTENT);

  if (status < 0) {
    log::error(
      "NtOpenFile() failed for '{}', {}",
      toString(poa), formatSystemMessage(status));

    return;
  }

  hc.add(oa.RootDirectory);
  unsigned char* buffer;

  if (depth >= buffers.size()) {
    buffers.emplace_back(std::make_unique<unsigned char[]>(AllocSize));
    buffer = buffers.back().get();
  } else {
    buffer = buffers[depth].get();
  }

  union
  {
    PVOID pv;
    PBYTE pb;
    PFILE_DIRECTORY_INFORMATION DirInfo;
  };

  for (;;) {
    status = NtQueryDirectoryFile(
      oa.RootDirectory, NULL, NULL, NULL, &iosb,
      buffer, AllocSize, FileDirectoryInformation, FALSE, NULL, FALSE);

    if (status == STATUS_NO_MORE_FILES) {
      break;
    } else if (status < 0) {
      log::error(
        "NtQueryDirectoryFile() failed for '{}', {}",
        toString(poa), formatSystemMessage(status));
      break;
    }

    ULONG NextEntryOffset = 0;

    pv = buffer;

    auto isDotDir = [](auto* o) {
      if (o->Length == 2 && o->Buffer[0] == '.') {
        return true;
      }

      if (o->Length == 4 && o->Buffer[0] == '.' && o->Buffer[1] == '.') {
        return true;
      }

      return false;
    };

    std::size_t count = 0;

    for (;;) {
      ++count;
      pb += NextEntryOffset;

      ObjectName.Buffer = DirInfo->FileName;
      ObjectName.Length = (USHORT)DirInfo->FileNameLength;

      if (!isDotDir(&ObjectName)) {
        ObjectName.MaximumLength = ObjectName.Length;

        if (DirInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
          dirStartF(cx, toStringView(&oa));
          forEachEntryImpl(cx, hc, buffers, &oa, depth+1, dirStartF, dirEndF, fileF);
          dirEndF(cx, toStringView(&oa));
        } else {
          fileF(cx, toStringView(&oa), toStdFileTime(DirInfo->LastWriteTime));
        }
      }

      NextEntryOffset = DirInfo->NextEntryOffset;

      if (NextEntryOffset == 0) {
        break;
      }
    }

    if (AllocSize - iosb.Information > (ULONG)FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, FileName[256])) {
      // NO_MORE_FILES
      break;
    }
  }
}


void DirectoryWalker::forEachEntry(
  const std::wstring& path, void* cx,
  DirStartF* dirStartF, DirEndF* dirEndF, FileF* fileF)
{
  auto& hc = g_handleClosers.request();

  if (!NtOpenFile) {
    LibraryPtr m(::LoadLibraryW(L"ntdll.dll"));
    NtOpenFile = (NtOpenFile_type)::GetProcAddress(m.get(), "NtOpenFile");
    NtQueryDirectoryFile = (NtQueryDirectoryFile_type)::GetProcAddress(m.get(), "NtQueryDirectoryFile");
    NtClose = (NtClose_type)::GetProcAddress(m.get(), "NtClose");
  }

  const std::wstring ntpath = std::wstring(L"\\??\\") + path;

  UNICODE_STRING ObjectName = {};
  ObjectName.Buffer = const_cast<wchar_t*>(ntpath.c_str());
  ObjectName.Length = (USHORT)ntpath.size() * sizeof(wchar_t);
  ObjectName.MaximumLength = ObjectName.Length;

  OBJECT_ATTRIBUTES oa = {};
  oa.Length = sizeof(oa);
  oa.ObjectName = &ObjectName;

  forEachEntryImpl(cx, hc, m_buffers, &oa, 0, dirStartF, dirEndF, fileF);
  hc.wakeup();
}

} // namespace
