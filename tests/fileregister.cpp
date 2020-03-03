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

  const fs::file_time_type ft = fs::file_time_type::clock::now();

  // add a file in root associated with the origin, not in an archive
  auto f1 = fr->addFile(*root, L"file1", origin, ft, {});
  ASSERT_TRUE(f1->getFileTime().has_value());
  EXPECT_EQ(f1->getFileTime(), ft);

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

TEST_F(FileRegisterTests, disableOrigin)
{
  using Files = std::vector<FileEntryPtr>;

  // builds a vector of the given origins in order of priority, assumes the
  // last is the primary, and checks that `f` has the correct primary and
  // alternatives
  //
  auto checkOrigins = [](FileEntryPtr f, auto&&... origins) {
    std::vector<OriginID> ids = {origins.getID()...};
    if (ids.empty()) {
      ids.push_back(InvalidOriginID);
    }

    // last item is the primary, rest are alternatives
    EXPECT_EQ(f->getOrigin(), ids.back());
    ids.pop_back();

    std::vector<OriginID> altIds;
    for (auto&& alt : f->getAlternatives()) {
      altIds.push_back(alt.originID);
    }

    EXPECT_EQ(altIds, ids);
  };

#define CHECK_ORIGINS(f, ...) \
  { \
    SCOPED_TRACE(f->getName()); \
    checkOrigins(f, __VA_ARGS__); \
  }


  // adding three origins, note the reverse order of priority so origin1 is
  // the highest
  auto& origin1 = fr->getOriginConnection()->createOrigin({
    L"origin one", L"c:\\origin one path", 3});

  auto& origin2 = fr->getOriginConnection()->createOrigin({
    L"origin two", L"c:\\origin two path", 2});

  auto& origin3 = fr->getOriginConnection()->createOrigin({
    L"origin three", L"c:\\origin three path", 1});


  //      origin1     origin2     origin3
  // f1      x
  // f2      x           x
  // f3      x           x           x
  // f4                  x           x
  // f5                              x


  // f1: origin1
  auto f1 = fr->addFile(*root, L"file1", origin1, {}, {});

  // f2: origin1, origin2
  auto f2 = fr->addFile(*root, L"file2", origin1, {}, {});
  fr->addFile(*root, f2->getName(), origin2, {}, {});

  // f3: origin1, origin2, origin3
  auto f3 = fr->addFile(*root, L"file3", origin1, {}, {});
  fr->addFile(*root, f3->getName(), origin2, {}, {});
  fr->addFile(*root, f3->getName(), origin3, {}, {});

  // f4: origin2, origin3
  auto f4 = fr->addFile(*root, L"file4", origin2, {}, {});
  fr->addFile(*root, f4->getName(), origin3, {}, {});

  // f5: origin3
  auto f5 = fr->addFile(*root, L"file5", origin3, {}, {});


  // making sure files are where they should
  EXPECT_EQ(fr->fileCount(), 5);
  EXPECT_EQ(origin1.getFiles(), Files({f1, f2, f3}));
  EXPECT_EQ(origin2.getFiles(), Files({f2, f3, f4}));
  EXPECT_EQ(origin3.getFiles(), Files({f3, f4, f5}));

  // making sure origins are correctly set in files; note that primary is last
  CHECK_ORIGINS(f1, origin1);
  CHECK_ORIGINS(f2, origin2, origin1);
  CHECK_ORIGINS(f3, origin3, origin2, origin1);
  CHECK_ORIGINS(f4, origin3, origin2);
  CHECK_ORIGINS(f5, origin3);


  // disable origin1
  fr->disableOrigin(origin1);

  // f1 is gone, rest are still there
  EXPECT_EQ(fr->fileCount(), 4);
  EXPECT_EQ(origin1.getFiles(), Files());
  EXPECT_EQ(origin2.getFiles(), Files({f2, f3, f4}));
  EXPECT_EQ(origin3.getFiles(), Files({f3, f4, f5}));

  // f1 has no origin, no files have origin1
  CHECK_ORIGINS(f1);
  CHECK_ORIGINS(f2, origin2);
  CHECK_ORIGINS(f3, origin3, origin2);
  CHECK_ORIGINS(f4, origin3, origin2);
  CHECK_ORIGINS(f5, origin3);


  // disable origin3
  fr->disableOrigin(origin3);

  // f5 is gone, f2, f3, and f4 are still there
  EXPECT_EQ(fr->fileCount(), 3);
  EXPECT_EQ(origin1.getFiles(), Files());
  EXPECT_EQ(origin2.getFiles(), Files({f2, f3, f4}));
  EXPECT_EQ(origin3.getFiles(), Files());

  // f1 and f5 have no origin, rest are all from origin2
  CHECK_ORIGINS(f1);
  CHECK_ORIGINS(f2, origin2);
  CHECK_ORIGINS(f3, origin2);
  CHECK_ORIGINS(f4, origin2);
  CHECK_ORIGINS(f5);


  // disable origin2
  fr->disableOrigin(origin2);

  // all files gone
  EXPECT_EQ(fr->fileCount(), 0);
  EXPECT_EQ(origin1.getFiles(), Files());
  EXPECT_EQ(origin2.getFiles(), Files());
  EXPECT_EQ(origin3.getFiles(), Files());

  // none of the files have any origin
  CHECK_ORIGINS(f1);
  CHECK_ORIGINS(f2);
  CHECK_ORIGINS(f3);
  CHECK_ORIGINS(f4);
  CHECK_ORIGINS(f5);

#undef CHECK_ORIGINS
}

} // namespace tests
