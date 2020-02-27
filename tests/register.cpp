#include "shared/fileentry.h"
#include "shared/directoryentry.h"
#include "shared/originconnection.h"
#include "shared/filesorigin.h"
#include <log.h>

namespace MOShared
{

TEST(FileEntry, CreateNoParent)
{
  auto e = FileEntry::create(1, L"name.ext", nullptr);

  EXPECT_EQ(e->getIndex(), 1);
  EXPECT_EQ(e->getName(), L"name.ext");
  EXPECT_TRUE(e->getAlternatives().empty());
  EXPECT_EQ(e->getOrigin(), InvalidOriginID);
  EXPECT_EQ(e->getArchive().name, L"");
  EXPECT_EQ(e->getArchive().order, InvalidOrder);
  EXPECT_EQ(e->getParent(), nullptr);

  // empty because the file has no origin
  EXPECT_TRUE(e->getFullPath().empty());
  EXPECT_TRUE(e->getFullPath(42).empty());

  // no parent directory, relative path is the filename only
  EXPECT_EQ(e->getRelativePath(), e->getName());

  // not from an archive
  EXPECT_FALSE(e->existsInArchive(L"some archive name"));
  EXPECT_FALSE(e->isFromArchive());

  // no time or size
  EXPECT_FALSE(e->getFileTime().has_value());
  EXPECT_FALSE(e->getFileSize().has_value());
  EXPECT_FALSE(e->getCompressedFileSize().has_value());
}

TEST(FileEntry, CreateInRoot)
{
  auto fr = FileRegister::create();
  auto d = DirectoryEntry::createRoot(fr);
  auto e = FileEntry::create(2, L"name.ext", d.get());

  EXPECT_EQ(e->getIndex(), 2);
  EXPECT_EQ(e->getName(), L"name.ext");
  EXPECT_TRUE(e->getAlternatives().empty());
  EXPECT_EQ(e->getOrigin(), InvalidOriginID);
  EXPECT_EQ(e->getArchive().name, L"");
  EXPECT_EQ(e->getArchive().order, InvalidOrder);
  EXPECT_EQ(e->getParent(), d.get());

  // empty because the file has no origin
  EXPECT_TRUE(e->getFullPath().empty());
  EXPECT_TRUE(e->getFullPath(42).empty());

  // relative path is filename only because it's in root
  EXPECT_EQ(e->getRelativePath(), e->getName());

  // not from an archive
  EXPECT_FALSE(e->existsInArchive(L"some archive name"));
  EXPECT_FALSE(e->isFromArchive());

  // no time or size
  EXPECT_FALSE(e->getFileTime().has_value());
  EXPECT_FALSE(e->getFileSize().has_value());
  EXPECT_FALSE(e->getCompressedFileSize().has_value());
}

TEST(FileEntry, CreateInDirectory)
{
  auto fr = FileRegister::create();
  auto root = DirectoryEntry::createRoot(fr);

  // creating a sub directory with no origin
  auto d = root->addSubDirectory(L"SubDir", L"subdir", InvalidOriginID);

  // creating a file inside that directory
  auto e = FileEntry::create(3, L"name.ext", d);
  EXPECT_EQ(e->getIndex(), 3);
  EXPECT_EQ(e->getName(), L"name.ext");
  EXPECT_EQ(e->getParent(), d);

  // no origins
  EXPECT_EQ(e->getOrigin(), InvalidOriginID);
  EXPECT_TRUE(e->getAlternatives().empty());

  // not from an archive
  EXPECT_EQ(e->getArchive().name, L"");
  EXPECT_EQ(e->getArchive().order, InvalidOrder);
  EXPECT_FALSE(e->existsInArchive(L"some archive name"));
  EXPECT_FALSE(e->isFromArchive());

  // empty because the file has no origin
  EXPECT_TRUE(e->getFullPath().empty());

  // full path from bad origin, must be empty
  EXPECT_TRUE(e->getFullPath(42).empty());

  // relative path only includes parent dir
  EXPECT_EQ(e->getRelativePath(), fs::path(d->getName()) / e->getName());

  // no time or size
  EXPECT_FALSE(e->getFileTime().has_value());
  EXPECT_FALSE(e->getFileSize().has_value());
  EXPECT_FALSE(e->getCompressedFileSize().has_value());
}

} // namespace
