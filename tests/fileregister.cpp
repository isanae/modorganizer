#include "fileentry.h"
#include "directoryentry.h"
#include "originconnection.h"
#include "filesorigin.h"

namespace tests
{

struct FileRegisterTests : public ::testing::Test
{
  std::shared_ptr<FileRegister> fr;
  std::unique_ptr<DirectoryEntry> root;

  FileRegisterTests()
  {
    fr = FileRegister::create();
    root = DirectoryEntry::createRoot(fr);
  }
};


TEST_F(FileRegisterTests, create)
{
  EXPECT_EQ(fr->fileCount(), 0);
  EXPECT_NE(fr->getOriginConnection().get(), nullptr);
}

TEST_F(FileRegisterTests, createFile)
{
  // note that the createFileInternal() calls create a desynced structure
  // because the files are not added back to the directory, but this shouldn't
  // matter

  auto f1 = fr->createFileInternal(L"file1", root.get());
  EXPECT_EQ(f1->getName(), L"file1");

  EXPECT_TRUE(fr->fileExists(f1->getIndex()));

  // `f1Again` should be the same object as `f1`
  auto f1Again = fr->getFile(f1->getIndex());
  ASSERT_TRUE(f1Again);
  EXPECT_EQ(f1, f1Again);
  EXPECT_EQ(fr->fileCount(), 1);


  // create() does not check for existing files, so that's a different file
  // object with the same name
  auto f2 = fr->createFileInternal(L"file2", root.get());
  EXPECT_EQ(f2->getName(), L"file2");

  // different indices
  EXPECT_NE(f1->getIndex(), f2->getIndex());

  // different objects
  EXPECT_NE(f1, f2);

  // make sure both are retrievable
  f1Again = fr->getFile(f1->getIndex());
  auto f2Again = fr->getFile(f2->getIndex());
  EXPECT_EQ(f1Again, f1);
  EXPECT_EQ(f2Again, f2);

  EXPECT_EQ(fr->fileCount(), 2);


  // although the structures are desynced, it should still be possible to
  // clean it up
  fr->removeFile(f1->getIndex());
  EXPECT_EQ(fr->fileCount(), 1);

  fr->removeFile(f2->getIndex());
  EXPECT_EQ(fr->fileCount(), 0);
}

TEST_F(FileRegisterTests, addAndRemoveFile)
{
  auto& origin = fr->getOriginConnection()->createOrigin({
    L"origin name", L"c:\\origin path", 1});

  const FILETIME ft = {1, 2};

  // add a file in root associated with the origin, not in an archive
  auto f1 = fr->addFile(*root, L"file1", origin, ft, {});
  ASSERT_TRUE(f1->getFileTime().has_value());
  EXPECT_EQ(f1->getFileTime()->dwLowDateTime, ft.dwLowDateTime);
  EXPECT_EQ(f1->getFileTime()->dwHighDateTime, ft.dwHighDateTime);

  // file must be in directory
  auto f1Again = root->findFile(f1->getName());
  ASSERT_TRUE(f1Again);
  EXPECT_EQ(f1, f1Again);

  // file must be in origin
  EXPECT_TRUE(origin.hasFile(f1->getIndex()));

  // origin must be in file
  EXPECT_EQ(f1->getOrigin(), origin.getID());


  // removing file
  fr->removeFile(f1->getIndex());

  // file must be gone from directory
  EXPECT_FALSE(root->findFile(f1->getName()));

  // file must be gone from origin
  EXPECT_TRUE(origin.getFileIndices().empty());

  // origin must be gone from file
  EXPECT_EQ(f1->getOrigin(), InvalidOriginID);
}

TEST_F(FileRegisterTests, changeFileOrigin)
{
  // adding two origins
  auto& origin1 = fr->getOriginConnection()->createOrigin({
    L"origin one", L"c:\\origin one path", 1});

  auto& origin2 = fr->getOriginConnection()->createOrigin({
    L"origin two", L"c:\\origin two path", 2});


  // creating a file in origin1
  auto f = fr->addFile(*root, L"file1", origin1, {}, {});

  // making sure it's really there
  EXPECT_EQ(root->findFile(f->getName()), f);
  EXPECT_TRUE(origin1.hasFile(f->getIndex()));
  EXPECT_EQ(f->getOrigin(), origin1.getID());


  // change the origin
  fr->changeFileOrigin(*f, origin1, origin2);

  // still in the same directory
  EXPECT_EQ(root->findFile(f->getName()), f);

  // gone from origin1
  EXPECT_FALSE(origin1.hasFile(f->getIndex()));

  // now in origin2
  EXPECT_TRUE(origin2.hasFile(f->getIndex()));
  EXPECT_EQ(f->getOrigin(), origin2.getID());


  // try moving it again, this should fail gracefully
  fr->changeFileOrigin(*f, origin1, origin2);

  // same tests as above
  EXPECT_EQ(root->findFile(f->getName()), f);
  EXPECT_FALSE(origin1.hasFile(f->getIndex()));
  EXPECT_TRUE(origin2.hasFile(f->getIndex()));
  EXPECT_EQ(f->getOrigin(), origin2.getID());


  // move it back to origin1
  fr->changeFileOrigin(*f, origin2, origin1);

  // still in root
  EXPECT_EQ(root->findFile(f->getName()), f);

  // gone from origin2
  EXPECT_FALSE(origin2.hasFile(f->getIndex()));

  // back in origin1
  EXPECT_TRUE(origin1.hasFile(f->getIndex()));
  EXPECT_EQ(f->getOrigin(), origin1.getID());
}

} // namespace tests
