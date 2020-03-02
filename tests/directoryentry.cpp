#include "fileentry.h"
#include "directoryentry.h"
#include "originconnection.h"
#include "filesorigin.h"

namespace tests
{

struct DirectoryEntryTests : public ::testing::Test
{
  std::shared_ptr<FileRegister> fr;
  std::unique_ptr<DirectoryEntry> root;

  DirectoryEntryTests()
  {
    fr = FileRegister::create();
    root = DirectoryEntry::createRoot(fr);
  }
};

using Files = std::vector<FileEntryPtr>;
using Dirs = std::vector<std::unique_ptr<DirectoryEntry>>;


TEST_F(DirectoryEntryTests, createRoot)
{
  // `root` was created in the fixture

  EXPECT_TRUE(root->isTopLevel());
  EXPECT_TRUE(root->isEmpty());
  EXPECT_FALSE(root->hasFiles());
  EXPECT_EQ(root->getParent(), nullptr);

  EXPECT_EQ(root->getName(), L"data");
  EXPECT_EQ(root->getFiles(), Files());
  EXPECT_EQ(root->getSubDirectories(), Dirs());

  EXPECT_EQ(root->getFileRegister(), fr);
}

TEST_F(DirectoryEntryTests, isTopLevel)
{
  auto d = root->addSubDirectory(L"sub", L"sub", 1);

  EXPECT_TRUE(root->isTopLevel());
  EXPECT_FALSE(d->isTopLevel());
}

TEST_F(DirectoryEntryTests, isEmpty)
{
  // this adds three directories in root: one with a sub directory, one with
  // a file, and one with both

  // root starts empty
  EXPECT_TRUE(root->isEmpty());

  // add 3 dirs in root
  auto subWithFiles = root->addSubDirectory(L"files", L"files", 1);
  auto subWithDirs = root->addSubDirectory(L"dirs", L"dirs", 1);
  auto subWithBoth = root->addSubDirectory(L"both", L"both", 1);

  // root isn't empty anymore
  EXPECT_FALSE(root->isEmpty());

  // all 3 are empty
  EXPECT_TRUE(subWithFiles->isEmpty());
  EXPECT_TRUE(subWithDirs->isEmpty());
  EXPECT_TRUE(subWithBoth->isEmpty());

  // add a file
  subWithFiles->addFileInternal(L"file");
  EXPECT_FALSE(subWithFiles->isEmpty());
  EXPECT_TRUE(subWithDirs->isEmpty());
  EXPECT_TRUE(subWithBoth->isEmpty());

  // add a subdir
  subWithDirs->addSubDirectory(L"sub", L"sub", 1);
  EXPECT_FALSE(subWithFiles->isEmpty());
  EXPECT_FALSE(subWithDirs->isEmpty());
  EXPECT_TRUE(subWithBoth->isEmpty());

  // add both
  subWithBoth->addFileInternal(L"file");
  subWithBoth->addSubDirectory(L"sub", L"sub", 1);
  EXPECT_FALSE(subWithFiles->isEmpty());
  EXPECT_FALSE(subWithDirs->isEmpty());
  EXPECT_FALSE(subWithBoth->isEmpty());
}

TEST_F(DirectoryEntryTests, hasFiles)
{
  EXPECT_FALSE(root->hasFiles());

  // adding a subdir
  root->addSubDirectory(L"sub", L"sub", 1);

  // no change
  EXPECT_FALSE(root->hasFiles());

  // adding a file
  root->addFileInternal(L"file");

  // now true
  EXPECT_TRUE(root->hasFiles());
}

TEST_F(DirectoryEntryTests, getParent)
{
  // root never has a parent
  EXPECT_EQ(root->getParent(), nullptr);

  // add a directory in `root`
  auto d = root->addSubDirectory(L"sub", L"sub", 1);
  EXPECT_EQ(d->getParent(), root.get());

  // add a directory in `d`
  auto d2 = d->addSubDirectory(L"sub2", L"sub2", 1);
  EXPECT_EQ(d2->getParent(), d);
}

TEST_F(DirectoryEntryTests, getName)
{
  EXPECT_EQ(root->getName(), L"data");

  // the lowercase version is only used for lookups, SubDir is the actual name
  auto d = root->addSubDirectory(L"SubDir", L"subdir", 1);
  EXPECT_EQ(d->getName(), L"SubDir");
}

TEST_F(DirectoryEntryTests, getFiles)
{
  // creating an origin for the files
  auto& o = fr->getOriginConnection()->createOrigin(
    {L"origin", L"c:\\origin", 1});

  // root starts empty
  EXPECT_EQ(root->getFiles().size(), 0);

  // adding one file
  auto f = fr->addFile(*root, L"file", o, {}, {});

  // file is in root
  EXPECT_EQ(root->getFiles(), Files({f}));

  // adding another
  auto f2 = fr->addFile(*root, L"file2", o, {}, {});

  // both files are in root
  EXPECT_EQ(root->getFiles(), Files({f, f2}));
}

TEST_F(DirectoryEntryTests, getSubDirectories)
{
  // root starts empty
  EXPECT_EQ(root->getSubDirectories().size(), 0);

  // adding one dir
  auto* d1 = root->addSubDirectory(L"sub1", L"sub1", 1);

  // dir is in root
  ASSERT_EQ(root->getSubDirectories().size(), 1);
  EXPECT_EQ(root->getSubDirectories()[0].get(), d1);

  // adding another
  auto* d2 = root->addSubDirectory(L"sub2", L"sub2", 1);

  // both dirs are in root
  ASSERT_EQ(root->getSubDirectories().size(), 2);
  EXPECT_EQ(root->getSubDirectories()[0].get(), d1);
  EXPECT_EQ(root->getSubDirectories()[1].get(), d2);
}

TEST_F(DirectoryEntryTests, getFileRegister)
{
  EXPECT_EQ(root->getFileRegister(), fr);
}

TEST_F(DirectoryEntryTests, getOriginConnection)
{
  EXPECT_EQ(root->getOriginConnection(), fr->getOriginConnection());
}

} // namespace tests
