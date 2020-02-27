#include "shared/fileentry.h"
#include "shared/directoryentry.h"
#include "shared/originconnection.h"
#include "shared/filesorigin.h"
#include <log.h>

namespace MOShared
{

struct FileEntryTests : public ::testing::Test
{
  std::shared_ptr<FileRegister> fr;
  std::unique_ptr<DirectoryEntry> root;

  FileEntryTests()
  {
    fr = FileRegister::create();
    root = DirectoryEntry::createRoot(fr);
  }
};


TEST_F(FileEntryTests, CreateNoParent)
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

TEST_F(FileEntryTests, CreateInRoot)
{
  auto e = FileEntry::create(2, L"name.ext", root.get());

  EXPECT_EQ(e->getIndex(), 2);
  EXPECT_EQ(e->getName(), L"name.ext");
  EXPECT_TRUE(e->getAlternatives().empty());
  EXPECT_EQ(e->getOrigin(), InvalidOriginID);
  EXPECT_EQ(e->getArchive().name, L"");
  EXPECT_EQ(e->getArchive().order, InvalidOrder);
  EXPECT_EQ(e->getParent(), root.get());

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

TEST_F(FileEntryTests, CreateInDirectory)
{
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

TEST_F(FileEntryTests, SingleOrigin)
{
  const auto& origin = fr->getOriginConnection()->createOrigin(
    L"origin one", "c:\\origin one path", 0);

  // creating a sub directory from this origin
  auto d = root->addSubDirectory(L"SubDir", L"subdir", origin.getID());

  // creating a file inside that directory
  auto e = FileEntry::create(4, L"name.ext", d);

  // adding the origin to the file; note that this doesn't add the file to the
  // origin, which isn't a problem for this test
  e->addOrigin({origin.getID(), {}}, {});

  EXPECT_EQ(e->getIndex(), 4);
  EXPECT_EQ(e->getName(), L"name.ext");
  EXPECT_EQ(e->getParent(), d);
  EXPECT_EQ(e->getOrigin(), origin.getID());

  // no alternatives
  EXPECT_TRUE(e->getAlternatives().empty());

  // not from an archive
  EXPECT_EQ(e->getArchive().name, L"");
  EXPECT_EQ(e->getArchive().order, InvalidOrder);
  EXPECT_FALSE(e->existsInArchive(L"some archive name"));
  EXPECT_FALSE(e->isFromArchive());

  // full path is the origin + directories + file name; note that the root
  // directory is not included
  EXPECT_EQ(e->getFullPath(), origin.getPath() / d->getName() / e->getName());

  // full path from bad origin, must be empty
  EXPECT_TRUE(e->getFullPath(42).empty());

  // relative path only includes parent dir
  EXPECT_EQ(e->getRelativePath(), fs::path(d->getName()) / e->getName());

  // no time or size
  EXPECT_FALSE(e->getFileTime().has_value());
  EXPECT_FALSE(e->getFileSize().has_value());
  EXPECT_FALSE(e->getCompressedFileSize().has_value());
}

TEST_F(FileEntryTests, OriginManipulation)
{
  // creating five origins in order of priority
  std::array<FilesOrigin*, 5> origins = {
    &fr->getOriginConnection()->createOrigin(
      L"origin zero", "c:\\origin zero path", 0),

    &fr->getOriginConnection()->createOrigin(
      L"origin one", "c:\\origin one path", 1),

    &fr->getOriginConnection()->createOrigin(
      L"origin two", "c:\\origin two path", 2),

    &fr->getOriginConnection()->createOrigin(
      L"origin three", "c:\\origin three path", 3),

    &fr->getOriginConnection()->createOrigin(
      L"origin four", "c:\\origin four path", 4)
  };


  // origins 0 and 4 will be from archives
  const ArchiveInfo origin0Archive(L"origin zero archive", 1);
  const ArchiveInfo origin4Archive(L"origin four archive", 2);

  // creating a sub directory from origin 2
  auto d = root->addSubDirectory(L"SubDir", L"subdir", origins[2]->getID());

  // creating a file inside that directory
  auto e = FileEntry::create(5, L"name.ext", d);

  EXPECT_EQ(e->getIndex(), 5);
  EXPECT_EQ(e->getName(), L"name.ext");
  EXPECT_EQ(e->getParent(), d);


  // this will add origins 2, 1, 3, 0, 4 in order
  //
  // the primary origin will change to 2, 1, 1, 0, 0
  //
  // origins 0 and 4 are from archives


  // adding origin 2 to the file; note that this doesn't add the file to the
  // origin, which isn't a problem  for this test
  e->addOrigin({origins[2]->getID(), {}}, {});
  EXPECT_EQ(e->getOrigin(), origins[2]->getID());

  // no alternatives
  EXPECT_TRUE(e->getAlternatives().empty());

  // full path is the origin + directories + file name; note that the root
  // directory is not included
  EXPECT_EQ(
    e->getFullPath(),
    origins[2]->getPath() / d->getName() / e->getName());

  // same as above because origin 2 is the primary
  EXPECT_EQ(
    e->getFullPath(origins[2]->getID()),
    origins[2]->getPath() / d->getName() / e->getName());

  // full path from bad origin, must be empty
  EXPECT_TRUE(e->getFullPath(42).empty());


  // adding another origin with a lower priority, will end up in the
  // alternatives
  e->addOrigin({origins[1]->getID(), {}}, {});
  EXPECT_EQ(e->getOrigin(), origins[2]->getID());

  EXPECT_EQ(e->getAlternatives(), std::vector<OriginInfo>({
    {origins[1]->getID(), {}}
  }));

  // full path from the new origin
  EXPECT_EQ(
    e->getFullPath(origins[1]->getID()),
    origins[1]->getPath() / d->getName() / e->getName());


  // adding another origin with a higher priority, will move the primary to
  // alternatives and set this origin as the new primary
  e->addOrigin({origins[3]->getID(), {}}, {});
  EXPECT_EQ(e->getOrigin(), origins[3]->getID());

  // alternatives are always sorted
  EXPECT_EQ(e->getAlternatives(), std::vector<OriginInfo>({
    {origins[1]->getID(), {}},
    {origins[2]->getID(), {}},
  }));

  // full path from the new origin
  EXPECT_EQ(
    e->getFullPath(origins[3]->getID()),
    origins[3]->getPath() / d->getName() / e->getName());


  // adding another origin from an archive with a lower priority, will end up
  // in the alternatives
  e->addOrigin({origins[0]->getID(), origin0Archive}, {});
  EXPECT_EQ(e->getOrigin(), origins[3]->getID());

  EXPECT_EQ(e->getAlternatives(), std::vector<OriginInfo>({
    {origins[0]->getID(), origin0Archive},
    {origins[1]->getID(), {}},
    {origins[2]->getID(), {}}
  }));

  // full path from the new origin
  EXPECT_EQ(
    e->getFullPath(origins[0]->getID()),
    origins[0]->getPath() / d->getName() / e->getName());

  // now has one origin from an archive, but not the primary
  EXPECT_EQ(e->getArchive().name, L"");                  // primary
  EXPECT_EQ(e->getArchive().order, InvalidOrder);        // primary
  EXPECT_FALSE(e->isFromArchive());                      // primary
  EXPECT_TRUE(e->existsInArchive(origin0Archive.name));  // alt
  EXPECT_FALSE(e->existsInArchive(L"bad archive name")); // bad alt


  // adding another origin from an archive with a higher priority, will move
  // the primary to alternatives and set this origin as the new primary
  e->addOrigin({origins[4]->getID(), origin4Archive}, {});
  EXPECT_EQ(e->getOrigin(), origins[4]->getID());

  EXPECT_EQ(e->getAlternatives(), std::vector<OriginInfo>({
    {origins[0]->getID(), origin0Archive},
    {origins[1]->getID(), {}},
    {origins[2]->getID(), {}},
    {origins[3]->getID(), {}},
  }));

  // full path from the new origin
  EXPECT_EQ(
    e->getFullPath(origins[4]->getID()),
    origins[4]->getPath() / d->getName() / e->getName());

  // primary is now from an archive
  EXPECT_EQ(e->getArchive().name, origin4Archive.name);  // primary
  EXPECT_EQ(e->getArchive().order, origin4Archive.order);// primary
  EXPECT_TRUE(e->isFromArchive());                       // primary
  EXPECT_TRUE(e->existsInArchive(origin4Archive.name));  // primary
  EXPECT_TRUE(e->existsInArchive(origin0Archive.name));  // alt
  EXPECT_FALSE(e->existsInArchive(L"bad archive name")); // bad alt


  // removing origin 2 from the alternatives
  EXPECT_FALSE(e->removeOrigin(origins[2]->getID()));
  EXPECT_EQ(e->getOrigin(), origins[4]->getID());

  EXPECT_EQ(e->getAlternatives(), std::vector<OriginInfo>({
    {origins[0]->getID(), origin0Archive},
    {origins[1]->getID(), {}},
    {origins[3]->getID(), {}},
  }));

  // no change in archives, same tests as above
  EXPECT_EQ(e->getArchive().name, origin4Archive.name);  // primary
  EXPECT_EQ(e->getArchive().order, origin4Archive.order);// primary
  EXPECT_TRUE(e->isFromArchive());                       // primary
  EXPECT_TRUE(e->existsInArchive(origin4Archive.name));  // primary
  EXPECT_TRUE(e->existsInArchive(origin0Archive.name));  // alt
  EXPECT_FALSE(e->existsInArchive(L"bad archive name")); // bad alt


  // removing origin 4, which is currently the primary; will make origin 3 the
  // new primary
  EXPECT_FALSE(e->removeOrigin(origins[4]->getID()));
  EXPECT_EQ(e->getOrigin(), origins[3]->getID());

  EXPECT_EQ(e->getAlternatives(), std::vector<OriginInfo>({
    {origins[0]->getID(), origin0Archive},
    {origins[1]->getID(), {}}
  }));

  // primary is not an archive anymore
  EXPECT_EQ(e->getArchive().name, L"");                  // primary
  EXPECT_EQ(e->getArchive().order, InvalidOrder);        // primary
  EXPECT_FALSE(e->isFromArchive());                      // primary
  EXPECT_FALSE(e->existsInArchive(origin4Archive.name)); // gone
  EXPECT_TRUE(e->existsInArchive(origin0Archive.name));  // alt
  EXPECT_FALSE(e->existsInArchive(L"bad archive name")); // bad alt


  // removing origin 4 again, shouldn't do anything, same tests as above
  EXPECT_FALSE(e->removeOrigin(origins[4]->getID()));
  EXPECT_EQ(e->getOrigin(), origins[3]->getID());

  EXPECT_EQ(e->getAlternatives(), std::vector<OriginInfo>({
    {origins[0]->getID(), origin0Archive},
    {origins[1]->getID(), {}}
  }));


  // removing origin 1 from alternatives
  EXPECT_FALSE(e->removeOrigin(origins[1]->getID()));
  EXPECT_EQ(e->getOrigin(), origins[3]->getID());

  EXPECT_EQ(e->getAlternatives(), std::vector<OriginInfo>({
    {origins[0]->getID(), origin0Archive},
  }));

  // no change in archives, same tests as above
  EXPECT_EQ(e->getArchive().name, L"");                  // primary
  EXPECT_EQ(e->getArchive().order, InvalidOrder);        // primary
  EXPECT_FALSE(e->isFromArchive());                      // primary
  EXPECT_FALSE(e->existsInArchive(origin4Archive.name)); // gone
  EXPECT_TRUE(e->existsInArchive(origin0Archive.name));  // alt
  EXPECT_FALSE(e->existsInArchive(L"bad archive name")); // bad alt


  // removing origin 3, which is current the primary; will make origin 0 the
  // new primary and empty the alternatives
  EXPECT_FALSE(e->removeOrigin(origins[3]->getID()));
  EXPECT_EQ(e->getOrigin(), origins[0]->getID());
  EXPECT_TRUE(e->getAlternatives().empty());

  // primary is from an archive again
  EXPECT_EQ(e->getArchive().name, origin0Archive.name);  // primary
  EXPECT_EQ(e->getArchive().order, origin0Archive.order);// primary
  EXPECT_TRUE(e->isFromArchive());                       // primary
  EXPECT_FALSE(e->existsInArchive(origin4Archive.name)); // gone
  EXPECT_TRUE(e->existsInArchive(origin0Archive.name));  // primary
  EXPECT_FALSE(e->existsInArchive(L"bad archive name")); // bad alt


  // remove origin 0; this is the last origin, so removeOrigin() will return
  // true
  EXPECT_TRUE(e->removeOrigin(origins[0]->getID()));
  EXPECT_EQ(e->getOrigin(), InvalidOriginID);
  EXPECT_TRUE(e->getAlternatives().empty());

  // no more origins
  EXPECT_EQ(e->getArchive().name, L"");                  // primary
  EXPECT_EQ(e->getArchive().order, InvalidOrder);        // primary
  EXPECT_FALSE(e->isFromArchive());                      // primary
  EXPECT_FALSE(e->existsInArchive(origin4Archive.name)); // gone
  EXPECT_FALSE(e->existsInArchive(origin0Archive.name)); // primary
  EXPECT_FALSE(e->existsInArchive(L"bad archive name")); // bad alt
}

TEST_F(FileEntryTests, OriginSorting)
{
  // a file entry will keep its origins sorted when adding or removing them,
  // but sortOrigins() must be called when the origins themselves change
  // priorities

  // creating five origins in order of priority
  std::array<FilesOrigin*, 5> origins = {
    &fr->getOriginConnection()->createOrigin(
      L"origin zero", "c:\\origin zero path", 0),

    &fr->getOriginConnection()->createOrigin(
      L"origin one", "c:\\origin one path", 0),

    &fr->getOriginConnection()->createOrigin(
      L"origin two", "c:\\origin two path", 1),

    &fr->getOriginConnection()->createOrigin(
      L"origin three", "c:\\origin three path", 2),

    &fr->getOriginConnection()->createOrigin(
      L"origin four", "c:\\origin four path", 2)
  };

  // origins 1 and 3 are from archives
  std::array<ArchiveInfo, 5> archives = {{
    {},
    {L"origin one archive", 1},
    {},
    {L"origin three archive", 2},
    {}
  }};


  // prios are 0, 0, 1, 2, 2
  // order is  1, 0, 2, 3, 4


  // creating a file inside the root
  auto e = FileEntry::create(1, L"name.ext", root.get());

  // adding all origins
  for (std::size_t i=0; i<origins.size(); ++i) {
    e->addOrigin({origins[i]->getID(), archives[i]}, {});
  }

  // origin 4 is primary because it has the highest prio along with origin 3,
  // but it's not from an archive, while origin 3 is
  EXPECT_EQ(e->getOrigin(), origins[4]->getID());
  EXPECT_EQ(e->getArchive(), archives[4]);

  // origin 1 is from an archive, so it gets sorted lower than origin 0 even
  // if they have the same prio
  EXPECT_EQ(e->getAlternatives(), std::vector<OriginInfo>({
    {origins[1]->getID(), archives[1]},
    {origins[0]->getID(), archives[0]},
    {origins[2]->getID(), archives[2]},
    {origins[3]->getID(), archives[3]}
  }));


  // prios are 0, 0, 1, 2, 2
  // order is  1, 0, 2, 3, 4


  // make origin 1 have the same prio has origin 2; will be lower than 2
  // because it's from an archive
  origins[1]->setPriority(1);
  e->sortOrigins();

  // primary doesn't change, alts have moved around
  EXPECT_EQ(e->getOrigin(), origins[4]->getID());
  EXPECT_EQ(e->getArchive(), archives[4]);

  EXPECT_EQ(e->getAlternatives(), std::vector<OriginInfo>({
    {origins[0]->getID(), archives[0]},
    {origins[1]->getID(), archives[1]},
    {origins[2]->getID(), archives[2]},
    {origins[3]->getID(), archives[3]}
  }));


  // prios are 0, 1, 1, 2, 2
  // order is  0, 1, 2, 3, 4


  // make origin 4 have a prio of 0; will be moved to the front and origin 3
  // will now be the primary
  origins[4]->setPriority(0);
  e->sortOrigins();

  // primary doesn't change, alts have moved around
  EXPECT_EQ(e->getOrigin(), origins[3]->getID());
  EXPECT_EQ(e->getArchive(), archives[3]);

  EXPECT_EQ(e->getAlternatives(), std::vector<OriginInfo>({
    {origins[0]->getID(), archives[0]},
    {origins[4]->getID(), archives[4]},
    {origins[1]->getID(), archives[1]},
    {origins[2]->getID(), archives[2]}
  }));


  // prios are 0, 1, 1, 2, 0
  // order is  0, 4, 1, 2, 3


  // make origin 1 have a prio of 4; will become primary
  origins[1]->setPriority(4);
  e->sortOrigins();

  // primary now 1, 3 the next highest
  EXPECT_EQ(e->getOrigin(), origins[1]->getID());
  EXPECT_EQ(e->getArchive(), archives[1]);

  EXPECT_EQ(e->getAlternatives(), std::vector<OriginInfo>({
    {origins[0]->getID(), archives[0]},
    {origins[4]->getID(), archives[4]},
    {origins[2]->getID(), archives[2]},
    {origins[3]->getID(), archives[3]}
  }));


  // prios are 0, 4, 1, 2, 0
  // order is  0, 4, 2, 3, 1
}

} // namespace
