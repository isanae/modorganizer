#include "fileentry.h"
#include "directoryentry.h"
#include "originconnection.h"
#include "filesorigin.h"

namespace tests
{

struct OriginConnectionTests : public ::testing::Test
{
  std::shared_ptr<FileRegister> fr;
  std::shared_ptr<OriginConnection> oc;

  OriginConnectionTests()
  {
    fr = FileRegister::create();
    oc = fr->getOriginConnection();
  }
};


TEST_F(OriginConnectionTests, getOrCreate)
{
  // create an origin first
  const auto& o = oc->getOrCreateOrigin({L"name", "C:\\origin dir", 1});
  EXPECT_EQ(o.getFileRegister().get(), fr.get());

  EXPECT_EQ(o.getName(), L"name");
  EXPECT_EQ(o.getPath(), "C:\\origin dir");
  EXPECT_EQ(o.getPriority(), 1);

  // then call getOrCreateOrigin() with the same name, but with different
  // parameters; those should be ignored because the origin is looked up by
  // name first
  const auto& o2 = oc->getOrCreateOrigin({L"name", "C:\\other path", 3});

  // should be the same object
  EXPECT_EQ(&o2, &o);

  // same tests as above
  EXPECT_EQ(o2.getName(), L"name");
  EXPECT_EQ(o2.getPath(), "C:\\origin dir");
  EXPECT_EQ(o2.getPriority(), 1);
}

TEST_F(OriginConnectionTests, create)
{
  // create an origin first
  const auto& o = oc->createOrigin({L"name", "C:\\origin dir", 1});
  EXPECT_EQ(o.getFileRegister().get(), fr.get());

  EXPECT_EQ(o.getName(), L"name");
  EXPECT_EQ(o.getPath(), "C:\\origin dir");
  EXPECT_EQ(o.getPriority(), 1);

  // createOrigin() doesn't check if the origin already exists, so the origin
  // above will get replaced
  //
  // note that this shouldn't normally happen because createOrigin() should
  // only be called when the origin doesn't exist, but let's make sure the
  // behaviour is consistent
  const auto& o2 = oc->createOrigin({L"name", "C:\\other path", 3});

  // from this point, `o` has been destroyed

  // should never be the same object
  EXPECT_NE(&o2, &o);

  // parameters must have changed too
  EXPECT_EQ(o2.getName(), L"name");
  EXPECT_EQ(o2.getPath(), "C:\\other path");
  EXPECT_EQ(o2.getPriority(), 3);
}

TEST_F(OriginConnectionTests, exists)
{
  EXPECT_FALSE(oc->exists(L""));
  EXPECT_FALSE(oc->exists(L"non-existent"));
  EXPECT_FALSE(oc->exists(L"origin 1"));
  EXPECT_FALSE(oc->exists(L"origin 2"));

  // add two origins
  oc->createOrigin({L"origin 1", "C:\\origin 1 path", 1});
  oc->createOrigin({L"origin 2", "C:\\origin 2 path", 2});

  EXPECT_FALSE(oc->exists(L""));
  EXPECT_FALSE(oc->exists(L"non-existent"));
  EXPECT_TRUE(oc->exists(L"origin 1"));
  EXPECT_TRUE(oc->exists(L"origin 2"));
}

TEST_F(OriginConnectionTests, findByID)
{
  // add two origins
  const auto& o1 = oc->createOrigin({L"origin 1", {}, 1});
  const auto& o2 = oc->createOrigin({L"origin 2", {}, 2});

  EXPECT_EQ(oc->findByID(o1.getID()), &o1);
  EXPECT_EQ(oc->findByID(o2.getID()), &o2);

  // non-existent
  EXPECT_EQ(oc->findByID(999), nullptr);
}

TEST_F(OriginConnectionTests, findByName)
{
  // add two origins
  const auto& o1 = oc->createOrigin({L"origin 1", {}, 1});
  const auto& o2 = oc->createOrigin({L"origin 2", {}, 2});

  EXPECT_EQ(oc->findByName(o1.getName()), &o1);
  EXPECT_EQ(oc->findByName(o2.getName()), &o2);

  // non-existent
  EXPECT_EQ(oc->findByName(L"non-existent"), nullptr);
}

TEST_F(OriginConnectionTests, changeNameLoookup)
{
  // add two origins
  const auto& o1 = oc->createOrigin({L"origin 1", {}, 1});
  const auto& o2 = oc->createOrigin({L"origin 2", {}, 2});

  EXPECT_EQ(oc->findByName(L"origin 1"), &o1);
  EXPECT_EQ(oc->findByName(L"origin 2"), &o2);
  EXPECT_EQ(oc->findByName(L"origin 1 renamed"), nullptr); // non-existent yet
  EXPECT_EQ(oc->findByName(L"origin 2 renamed"), nullptr); // non-existent yet

  // rename origin 1
  oc->changeNameLookupInternal(L"origin 1", L"origin 1 renamed");

  EXPECT_EQ(oc->findByName(L"origin 1"), nullptr);         // dead
  EXPECT_EQ(oc->findByName(L"origin 2"), &o2);
  EXPECT_EQ(oc->findByName(L"origin 1 renamed"), &o1);     // old origin 1
  EXPECT_EQ(oc->findByName(L"origin 2 renamed"), nullptr); // non-existent yet

  // rename origin 2
  oc->changeNameLookupInternal(L"origin 2", L"origin 2 renamed");

  EXPECT_EQ(oc->findByName(L"origin 1"), nullptr);         // dead
  EXPECT_EQ(oc->findByName(L"origin 2"), nullptr);         // dead
  EXPECT_EQ(oc->findByName(L"origin 1 renamed"), &o1);     // old origin 1
  EXPECT_EQ(oc->findByName(L"origin 2 renamed"), &o2);     // old origin 2

  // rename origin 1 back to original
  oc->changeNameLookupInternal(L"origin 1 renamed", L"origin 1");

  EXPECT_EQ(oc->findByName(L"origin 1"), &o1);             // back to life
  EXPECT_EQ(oc->findByName(L"origin 2"), nullptr);         // dead
  EXPECT_EQ(oc->findByName(L"origin 1 renamed"), nullptr); // dead
  EXPECT_EQ(oc->findByName(L"origin 2 renamed"), &o2);     // old origin 2

  // no effect
  oc->changeNameLookupInternal(L"unknown origin", L"something else");

  // same tests as above
  EXPECT_EQ(oc->findByName(L"origin 1"), &o1);             // back to life
  EXPECT_EQ(oc->findByName(L"origin 2"), nullptr);         // dead
  EXPECT_EQ(oc->findByName(L"origin 1 renamed"), nullptr); // dead
  EXPECT_EQ(oc->findByName(L"origin 2 renamed"), &o2);     // old origin 2
}

} // namespace
