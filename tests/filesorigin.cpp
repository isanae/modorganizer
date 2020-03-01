#include "fileentry.h"
#include "directoryentry.h"
#include "originconnection.h"
#include "filesorigin.h"

namespace tests
{

struct FilesOriginTests : public ::testing::Test
{
  std::shared_ptr<FileRegister> fr;
  std::shared_ptr<OriginConnection> oc;
  std::shared_ptr<DirectoryEntry> root;

  FilesOriginTests()
  {
    fr = FileRegister::create();
    oc = fr->getOriginConnection();
    root = DirectoryEntry::createRoot(fr);
  }
};


TEST_F(FilesOriginTests, constructor)
{
  FilesOrigin o(1, {L"name", L"c:\\origin path", 2}, oc);

  EXPECT_EQ(o.getPriority(), 2);
  EXPECT_EQ(o.getName(), L"name");
  EXPECT_EQ(o.getID(), 1);
  EXPECT_TRUE(o.getFiles().empty());
  EXPECT_EQ(o.getOriginConnection().get(), oc.get());
  EXPECT_EQ(o.getFileRegister().get(), fr.get());
}

TEST_F(FilesOriginTests, setPriority)
{
  FilesOrigin o(1, {L"name", L"c:\\origin path", 2}, oc);
  EXPECT_EQ(o.getPriority(), 2);

  o.setPriority(3);
  EXPECT_EQ(o.getPriority(), 3);

  o.setPriority(0);
  EXPECT_EQ(o.getPriority(), 0);

  // negative priorities are ignored
  o.setPriority(-1);
  EXPECT_EQ(o.getPriority(), 0);
}

TEST_F(FilesOriginTests, setName)
{
  // note that changing the name does three things:
  //   - changes the name of the origin,
  //   - changes the last component of the path to the same value
  //   - calls OriginConnection::changeNameLookup()
  //
  // all three side effects are checked for each change

  auto& o = oc->createOrigin({L"origin1", L"c:\\somewhere\\origin1", 2});
  EXPECT_EQ(o.getName(), L"origin1");
  EXPECT_EQ(o.getPath(), L"c:\\somewhere\\origin1");
  EXPECT_EQ(oc->findByName(L"origin1"), &o);

  o.setName(L"origin2");
  EXPECT_EQ(o.getName(), L"origin2");
  EXPECT_EQ(o.getPath(), L"c:\\somewhere\\origin2");
  EXPECT_EQ(oc->findByName(L"origin1"), nullptr);  // gone
  EXPECT_EQ(oc->findByName(L"origin2"), &o);

  o.setName(L"origin3");
  EXPECT_EQ(o.getName(), L"origin3");
  EXPECT_EQ(o.getPath(), L"c:\\somewhere\\origin3");
  EXPECT_EQ(oc->findByName(L"origin1"), nullptr);  // gone
  EXPECT_EQ(oc->findByName(L"origin2"), nullptr);  // gone
  EXPECT_EQ(oc->findByName(L"origin3"), &o);

  // back to original
  o.setName(L"origin1");
  EXPECT_EQ(o.getName(), L"origin1");
  EXPECT_EQ(o.getPath(), L"c:\\somewhere\\origin1");
  EXPECT_EQ(oc->findByName(L"origin1"), &o);       // back
  EXPECT_EQ(oc->findByName(L"origin2"), nullptr);  // gone
  EXPECT_EQ(oc->findByName(L"origin3"), nullptr);  // gone

  // empty names are ignored, tests are the same as above
  o.setName(L"");
  EXPECT_EQ(o.getName(), L"origin1");
  EXPECT_EQ(o.getPath(), L"c:\\somewhere\\origin1");
  EXPECT_EQ(oc->findByName(L"origin1"), &o);       // back
  EXPECT_EQ(oc->findByName(L"origin2"), nullptr);  // gone
  EXPECT_EQ(oc->findByName(L"origin3"), nullptr);  // gone


  // the next tests are about changing the name of an origin to one that
  // already exists; this shouldn't happen, but the OriginConnection should
  // still clean up correctly


  // create a second origin
  auto& o2 = oc->createOrigin({L"origin2", L"c:\\somewhere\\origin2", 3});
  EXPECT_EQ(o2.getName(), L"origin2");
  EXPECT_EQ(o2.getPath(), L"c:\\somewhere\\origin2");
  EXPECT_EQ(oc->findByName(L"origin1"), &o);
  EXPECT_EQ(oc->findByName(L"origin2"), &o2);
  EXPECT_EQ(oc->findByName(L"origin3"), nullptr);  // gone

  // rename "origin1" to "origin2"; the rename will succeed, and origin2 will
  // be removed completely
  o.setName(L"origin2");

  EXPECT_EQ(o.getName(), L"origin2");
  EXPECT_EQ(o.getPath(), L"c:\\somewhere\\origin2");
  EXPECT_EQ(oc->findByName(L"origin1"), nullptr);  // gone
  EXPECT_EQ(oc->findByName(L"origin2"), &o);       // not `o2` anymore
  EXPECT_EQ(oc->findByName(L"origin3"), nullptr);  // gone
}

TEST_F(FilesOriginTests, files)
{
  // create an origin
  auto& o = oc->createOrigin({L"origin1", L"c:\\somewhere\\origin1", 1});

  // create three files in the root from that origin
  auto f0 = fr->addFile(*root, L"file0", o, {}, {});
  auto f1 = fr->addFile(*root, L"file1", o, {}, {});
  auto f2 = fr->addFile(*root, L"file2", o, {}, {});

  auto expectsFiles = [&](std::vector<FileEntry*> v) {
    const auto files = o.getFiles();
    ASSERT_EQ(files.size(), v.size());

    for (std::size_t i=0; i<files.size(); ++i) {
      EXPECT_EQ(files[i].get(), v[i]);
    }
  };

  auto expectsIndices = [&](std::set<FileIndex> set) {
    const auto indices = o.getFileIndices();
    ASSERT_EQ(indices, set);
  };


  // get them back
  expectsFiles({f0.get(), f1.get(), f2.get()});
  expectsIndices({f0->getIndex(), f1->getIndex(), f2->getIndex()});

  // add a non-existing file to it, should be skipped by getFiles(); note that
  // the structures are now desynced, but file 42 will be removed just below
  o.addFileInternal(42);
  expectsFiles({f0.get(), f1.get(), f2.get()});
  expectsIndices({42, f0->getIndex(), f1->getIndex(), f2->getIndex()});

  // remove a file that isn't in the origin, should be a no-op
  o.removeFileInternal(999);
  expectsFiles({f0.get(), f1.get(), f2.get()});
  expectsIndices({42, f0->getIndex(), f1->getIndex(), f2->getIndex()});

  // remove the non-existent file
  o.removeFileInternal(42);
  expectsFiles({f0.get(), f1.get(), f2.get()});
  expectsIndices({f0->getIndex(), f1->getIndex(), f2->getIndex()});

  // remove f1
  fr->removeFile(f1->getIndex());
  expectsFiles({f0.get(), f2.get()});
  expectsIndices({f0->getIndex(), f2->getIndex()});

  // disable the origin, all files should be gone
  fr->disableOrigin(o);

  // no more files, disabled, file registry empty
  expectsFiles({});
  expectsIndices({});
  EXPECT_EQ(fr->fileCount(), 0);
}

} // namespace
