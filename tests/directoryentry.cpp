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

TEST_F(DirectoryEntryTests, anyOrigin)
{
  auto oc = fr->getOriginConnection();

  // anyOrigin() will return in order:
  //  1) the origin of the first file that's not an archive
  //  2) the origin of a subdirectory
  //  3) the origin of this directory
  //  4) InvalidOriginID
  //
  // in this test, origins are added in reverse order to make sure they
  // override what was previously returned

  // origins must exist for comparison to work
  std::vector<OriginID> o;
  for (int i=0; i<7; ++i) {
    const auto name = L"origin" + std::to_wstring(i);
    FilesOrigin& fo = oc->createOrigin({name, L"C:\\" + name, i});
    o.push_back(fo.getID());
  }

  // root starts with DataOriginID
  EXPECT_EQ(root->anyOrigin(), DataOriginID);

  // add a dir from origin 0
  auto* d = root->addSubDirectory(L"d", L"d", o[0]);
  EXPECT_EQ(d->anyOrigin(), o[0]);

    // add a subdir from origin 1, picked up
    auto* sub1 = d->addSubDirectory(L"sub1", L"sub1", o[1]);
    EXPECT_EQ(d->anyOrigin(), o[1]);

    // add a file from origin 2, but from an archive; not picked up
    auto f = d->addFileInternal(L"file");
    f->addOriginInternal({o[2], {L"archive", 1}}, {});
    EXPECT_EQ(d->anyOrigin(), o[1]);

      // add an origin to the file that's not from an archive; picked up
      f->addOriginInternal({o[3], {}}, {});
      EXPECT_EQ(d->anyOrigin(), o[3]);

    // remove that file, back to origin 1
    d->removeFileInternal(f->getName());
    EXPECT_EQ(d->anyOrigin(), o[1]);

  // remove the subdir, back to origin 0
  d->removeSubDirectoryInternal(sub1->getName());
  EXPECT_EQ(d->anyOrigin(), o[0]);

  // root is also from origin  because of `d`
  EXPECT_EQ(root->anyOrigin(), o[0]);

  // remove `d`, root back to Data origin
  root->removeSubDirectoryInternal(d->getName());
  EXPECT_EQ(root->anyOrigin(), DataOriginID);
}

} // namespace tests
