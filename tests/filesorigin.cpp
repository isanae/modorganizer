#include "shared/fileentry.h"
#include "shared/directoryentry.h"
#include "shared/originconnection.h"
#include "shared/filesorigin.h"

namespace MOShared::tests
{

struct FilesOriginTests : public ::testing::Test
{
  std::shared_ptr<FileRegister> fr;
  std::shared_ptr<OriginConnection> oc;

  FilesOriginTests()
  {
    fr = FileRegister::create();
    oc = fr->getOriginConnection();
  }
};


TEST_F(FilesOriginTests, constructor)
{
  FilesOrigin o(1, L"name", L"c:\\origin path", 2, oc);

  EXPECT_EQ(o.getPriority(), 2);
  EXPECT_EQ(o.getName(), L"name");
  EXPECT_EQ(o.getID(), 1);
  EXPECT_TRUE(o.getFiles().empty());
  EXPECT_TRUE(o.isEnabled());
  EXPECT_EQ(o.getOriginConnection().get(), oc.get());
  EXPECT_EQ(o.getFileRegister().get(), fr.get());
}

TEST_F(FilesOriginTests, setPriority)
{
  FilesOrigin o(1, L"name", L"c:\\origin path", 2, oc);
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

  auto& o = oc->createOrigin(L"origin1", L"c:\\somewhere\\origin1", 2);
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
  auto& o2 = oc->createOrigin(L"origin2", L"c:\\somewhere\\origin2", 3);
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

} // namespace
