#include "shared/fileentry.h"
#include <gtest/gtest.h>

namespace MOShared
{

TEST(FileEntry, CreateNoParent)
{
  auto e = FileEntry::create(1, L"name", nullptr);
  EXPECT_EQ(e->getIndex(), 1);
  EXPECT_EQ(e->getName(), L"name");
  EXPECT_EQ(e->getParent(), nullptr);
}

} // namespace
