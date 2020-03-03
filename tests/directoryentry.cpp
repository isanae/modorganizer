#include "fileentry.h"
#include "directoryentry.h"
#include "originconnection.h"
#include "filesorigin.h"
#include "util.h"

namespace tests
{

struct DirectoryEntryTests : public ::testing::Test
{
  std::shared_ptr<FileRegister> fr;
  std::unique_ptr<DirectoryEntry> root;

  DirectoryEntry* meshes = nullptr;
  DirectoryEntry* effects = nullptr;
  FileEntryPtr blood_nif;
  DirectoryEntry* landscape = nullptr;
  DirectoryEntry* tree = nullptr;
  FileEntryPtr tree01_nif;
  FileEntryPtr tree02_nif;
  DirectoryEntry* textures = nullptr;
  DirectoryEntry* clutter = nullptr;
  FileEntryPtr barrel01_dds;
  DirectoryEntry* terrain = nullptr;
  FileEntryPtr noise_dds;

  DirectoryEntryTests()
  {
    fr = FileRegister::create();
    root = DirectoryEntry::createRoot(fr);
  }

  void createHierarchy()
  {
    // creates the following hierarchy
    //  +- root
    //     +- meshes
    //     |   +- effects
    //     |   |   +- blood.nif
    //     |   +- landscape
    //     |       +- tree
    //     |           +- tree01.nif
    //     |           +- tree02.nif
    //     +- textures
    //         +- clutter
    //         |   +- barrel01.dds
    //         +- terrain
    //             +- noise.dds

    meshes = root->addSubDirectory(L"meshes", 1);
    effects = meshes->addSubDirectory(L"effects", 1);
    blood_nif = effects->addFileInternal(L"blood.nif");
    landscape = meshes->addSubDirectory(L"landscape", 1);
    tree = landscape->addSubDirectory(L"tree", 1);
    tree01_nif = tree->addFileInternal(L"tree01.nif");
    tree02_nif = tree->addFileInternal(L"tree02.nif");
    textures = root->addSubDirectory(L"textures", 1);
    clutter = textures->addSubDirectory(L"clutter", 1);
    barrel01_dds = clutter->addFileInternal(L"barrel01.dds");
    terrain = textures->addSubDirectory(L"terrain", 1);
    noise_dds = terrain->addFileInternal(L"noise.dds");
  }

  // tests multiple case variations of `path`, make sure they all find `what`
  //
  void checkCaseVariations(
    DirectoryEntry* from, std::wstring path, FileEntryPtr what)
  {
    checkCaseVariationsImpl(
      from, path, what, &DirectoryEntry::findFileRecursive);
  }

  // tests multiple case variations of `path`, make sure they all find `what`
  //
  void checkCaseVariations(
    DirectoryEntry* from, std::wstring path, DirectoryEntry* what)
  {
    using MF = DirectoryEntry* (DirectoryEntry::*)(
      std::wstring_view path, bool alreadyLowerCase);

    checkCaseVariationsImpl(
      from, path, what,
      static_cast<MF>(&DirectoryEntry::findSubDirectoryRecursive));
  }

  // tests multiple case variations of `path`, make sure they all find `what`;
  // this is used by both findSubDirectoryRecursive() and findFileRecursive()
  // tests, and so the actual member function `mf` is a template parameter
  // pointing to either
  //
  template <class What, class MF>
  void checkCaseVariationsImpl(
    DirectoryEntry* from, std::wstring path, What what, MF mf)
  {
    SCOPED_TRACE(path);

    // uppercased path
    std::wstring uppercased;

    // whether the uppercased path has any character that's not a separator
    bool uppercasedEmpty = true;

    for (wchar_t c : path) {
      if (c != L'\\' && c != L'/') {
        uppercasedEmpty = false;
      }

      uppercased += std::toupper(c);
    }

    // lowercased path
    std::wstring lowercased = ToLowerCopy(path);


    // as-is
    EXPECT_EQ((from->*mf)(path, false), what);

    // uppercased
    EXPECT_EQ((from->*mf)(uppercased, false), what);

    // lowercased
    EXPECT_EQ((from->*mf)(lowercased, false), what);

    // lowercased, with the flag saying that it's already lowercase
    EXPECT_EQ((from->*mf)(lowercased, true), what);

    if (uppercasedEmpty) {
      // if the path is empty, this won't fail
      EXPECT_EQ((from->*mf)(uppercased, true), what);
    } else {
      // uppercased, but with the flag that it's actually lowercased; this
      // should never find anything because this test has fully uppercase
      // directory names
      EXPECT_EQ((from->*mf)(uppercased, true), nullptr);
    }
  };
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
  auto d = root->addSubDirectory(L"sub", 1);

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
  auto subWithFiles = root->addSubDirectory(L"files", 1);
  auto subWithDirs = root->addSubDirectory(L"dirs", 1);
  auto subWithBoth = root->addSubDirectory(L"both", 1);

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
  subWithDirs->addSubDirectory(L"sub", 1);
  EXPECT_FALSE(subWithFiles->isEmpty());
  EXPECT_FALSE(subWithDirs->isEmpty());
  EXPECT_TRUE(subWithBoth->isEmpty());

  // add both
  subWithBoth->addFileInternal(L"file");
  subWithBoth->addSubDirectory(L"sub", 1);
  EXPECT_FALSE(subWithFiles->isEmpty());
  EXPECT_FALSE(subWithDirs->isEmpty());
  EXPECT_FALSE(subWithBoth->isEmpty());
}

TEST_F(DirectoryEntryTests, hasFiles)
{
  EXPECT_FALSE(root->hasFiles());

  // adding a subdir
  root->addSubDirectory(L"sub", 1);

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
  auto d = root->addSubDirectory(L"sub", 1);
  EXPECT_EQ(d->getParent(), root.get());

  // add a directory in `d`
  auto d2 = d->addSubDirectory(L"sub2", 1);
  EXPECT_EQ(d2->getParent(), d);
}

TEST_F(DirectoryEntryTests, getName)
{
  EXPECT_EQ(root->getName(), L"data");

  // the lowercase version is only used for lookups, SubDir is the actual name
  auto d = root->addSubDirectory(L"SubDir", 1);
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
  auto* d1 = root->addSubDirectory(L"sub1", 1);

  // dir is in root
  ASSERT_EQ(root->getSubDirectories().size(), 1);
  EXPECT_EQ(root->getSubDirectories()[0].get(), d1);

  // adding another
  auto* d2 = root->addSubDirectory(L"sub2", 1);

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
  auto* d = root->addSubDirectory(L"d", o[0]);
  EXPECT_EQ(d->anyOrigin(), o[0]);

    // add a subdir from origin 1, picked up
    auto* sub1 = d->addSubDirectory(L"sub1", o[1]);
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

TEST_F(DirectoryEntryTests, forEachDirectory)
{
  // check that the callback isn't executed at all when there are no
  // directories
  bool executed = false;
  root->forEachDirectory([&](auto&) {
    executed = true;
    return true;
  });

  EXPECT_FALSE(executed);

  // create 5 directories in root, use names that are not in alphabetical order
  const std::vector<DirectoryEntry*> dirs = {
    root->addSubDirectory(L"d3", 1),
    root->addSubDirectory(L"d2", 1),
    root->addSubDirectory(L"d1", 1),
    root->addSubDirectory(L"d4", 1),
    root->addSubDirectory(L"d5", 1)
  };

  // make sure all 5 directories were given and in that they were ordered
  // correctly
  std::vector<DirectoryEntry*> seen;
  root->forEachDirectory([&](auto& d) {
    seen.push_back(&d);
    return true;
  });

  EXPECT_EQ(seen, dirs);

  // check that early stop works
  std::size_t i = 0;
  root->forEachDirectory([&](auto&) {
    ++i;

    if (i == 3) {
      return false;
    } else {
      return true;
    }
  });

  EXPECT_EQ(i, 3);
}

TEST_F(DirectoryEntryTests, forEachFile)
{
  // check that the callback isn't executed at all when there are no
  // files
  bool executed = false;
  root->forEachFile([&](auto&) {
    executed = true;
    return true;
  });

  EXPECT_FALSE(executed);

  // origin for the files
  auto& o = fr->getOriginConnection()->createOrigin({L"o", L"c:\\o", 1});

  // create 5 files in root, use names that are not in alphabetical order
  const std::vector<FileEntryPtr> files = {
    fr->addFile(*root, L"f3", o, {}, {}),
    fr->addFile(*root, L"f2", o, {}, {}),
    fr->addFile(*root, L"f1", o, {}, {}),
    fr->addFile(*root, L"f4", o, {}, {}),
    fr->addFile(*root, L"f5", o, {}, {})
  };

  // order them alphabetically
  const std::vector<FileIndex> indices = {
    files[2]->getIndex(),
    files[1]->getIndex(),
    files[0]->getIndex(),
    files[3]->getIndex(),
    files[4]->getIndex(),
  };

  // make sure all 5 files were given and in that they were ordered correctly
  std::vector<FileIndex> seen;
  root->forEachFile([&](auto& f) {
    seen.push_back(f.getIndex());
    return true;
  });

  EXPECT_EQ(seen, indices);

  // check that early stop works
  std::size_t i = 0;
  root->forEachFile([&](auto&) {
    ++i;

    if (i == 3) {
      return false;
    } else {
      return true;
    }
  });

  EXPECT_EQ(i, 3);
}

TEST_F(DirectoryEntryTests, forEachFileIndex)
{
  // check that the callback isn't executed at all when there are no
  // files
  bool executed = false;
  root->forEachFileIndex([&](auto&) {
    executed = true;
    return true;
  });

  EXPECT_FALSE(executed);

  // origin for the files
  auto& o = fr->getOriginConnection()->createOrigin({L"o", L"c:\\o", 1});

  // create 5 files in root, use names that are not in alphabetical order
  const std::vector<FileEntryPtr> files = {
    fr->addFile(*root, L"f3", o, {}, {}),
    fr->addFile(*root, L"f2", o, {}, {}),
    fr->addFile(*root, L"f1", o, {}, {}),
    fr->addFile(*root, L"f4", o, {}, {}),
    fr->addFile(*root, L"f5", o, {}, {})
  };

  // order them alphabetically
  const std::vector<FileIndex> indices = {
    files[2]->getIndex(),
    files[1]->getIndex(),
    files[0]->getIndex(),
    files[3]->getIndex(),
    files[4]->getIndex(),
  };

  // make sure all 5 files were given and in that they were ordered correctly
  std::vector<FileIndex> seen;
  root->forEachFileIndex([&](auto i) {
    seen.push_back(i);
    return true;
  });

  EXPECT_EQ(seen, indices);

  // check that early stop works
  std::size_t i = 0;
  root->forEachFileIndex([&](auto&) {
    ++i;

    if (i == 3) {
      return false;
    } else {
      return true;
    }
  });

  EXPECT_EQ(i, 3);
}

TEST_F(DirectoryEntryTests, findSubDirectoryName)
{
  // non-existing
  EXPECT_EQ(root->findSubDirectory(L"non-existing"), nullptr);

  // add one
  auto* d = root->addSubDirectory(L"SubDir", 1);

  // make sure it's there, case insensitive
  EXPECT_EQ(root->findSubDirectory(L"SubDir"), d);
  EXPECT_EQ(root->findSubDirectory(L"SUBDIR"), d);
  EXPECT_EQ(root->findSubDirectory(L"subdir"), d);
  EXPECT_EQ(root->findSubDirectory(L"non-existing"), nullptr);

  // remove it
  root->removeSubDirectoryInternal(d->getName());

  // make sure it's gone
  EXPECT_EQ(root->findSubDirectory(L"SubDir"), nullptr);
  EXPECT_EQ(root->findSubDirectory(L"SUBDIR"), nullptr);
  EXPECT_EQ(root->findSubDirectory(L"subdir"), nullptr);
  EXPECT_EQ(root->findSubDirectory(L"non-existing"), nullptr);
}

TEST_F(DirectoryEntryTests, findSubDirectoryKey)
{
  // non-existing
  EXPECT_EQ(root->findSubDirectory(FileKey(L"non-existing")), nullptr);

  // add one
  auto* d = root->addSubDirectory(L"SubDir", 1);

  // make sure it's there; FileKey assumes the value is lowercase, so this
  // is case sensitive
  EXPECT_EQ(root->findSubDirectory(FileKey(L"subdir")), d);
  EXPECT_EQ(root->findSubDirectory(FileKey(L"SubDir")), nullptr);
  EXPECT_EQ(root->findSubDirectory(FileKey(L"SUBDIR")), nullptr);
  EXPECT_EQ(root->findSubDirectory(FileKey(L"non-existing")), nullptr);

  // remove it
  root->removeSubDirectoryInternal(d->getName());

  // make sure it's gone
  EXPECT_EQ(root->findSubDirectory(FileKey(L"subdir")), nullptr);
  EXPECT_EQ(root->findSubDirectory(FileKey(L"SubDir")), nullptr);
  EXPECT_EQ(root->findSubDirectory(FileKey(L"SUBDIR")), nullptr);
  EXPECT_EQ(root->findSubDirectory(FileKey(L"non-existing")), nullptr);
}

TEST_F(DirectoryEntryTests, forEachPathComponent)
{
  using details::forEachPathComponent;

  auto check = [](std::wstring path, std::vector<std::wstring> split) {
    std::vector<std::wstring> components;

    forEachPathComponent(path, [&](auto&& c, bool last) {
      components.push_back(std::wstring(c.begin(), c.end()));

      if (last) {
        components.push_back(L"LAST");
      }

      return true;
    });

    if (!split.empty()) {
      split.push_back(L"LAST");
    }

    EXPECT_EQ(components, split);
  };

#define CHECK(PATH, SPLIT) \
  { \
    SCOPED_TRACE(PATH); \
    check(PATH, SPLIT); \
  }

  using V = std::vector<std::wstring>;

  CHECK(LR"()",            V());
  CHECK(LR"(/)",           V());
  CHECK(LR"(//)",          V());
  CHECK(LR"(\)",           V());
  CHECK(LR"(\\)",          V());

  CHECK(LR"(a)",           V({L"a"}));
  CHECK(LR"(a/)",          V({L"a"}));
  CHECK(LR"(a\)",          V({L"a"}));

  CHECK(LR"(/a)",          V({L"a"}));
  CHECK(LR"(//a/)",        V({L"a"}));
  CHECK(LR"(\//a\)",       V({L"a"}));

  CHECK(LR"(a/b)",         V({L"a", L"b"}));
  CHECK(LR"(a/b/)",        V({L"a", L"b"}));

  CHECK(LR"(a/b/c)",       V({L"a", L"b", L"c"}));
  CHECK(LR"(a//b//c)",     V({L"a", L"b", L"c"}));
  CHECK(LR"(a\b/c)",       V({L"a", L"b", L"c"}));
  CHECK(LR"(a\b//c)",      V({L"a", L"b", L"c"}));
  CHECK(LR"(a\b//\c)",     V({L"a", L"b", L"c"}));

  CHECK(LR"(/a/b/c)",      V({L"a", L"b", L"c"}));
  CHECK(LR"(//a//b//c)",   V({L"a", L"b", L"c"}));
  CHECK(LR"(\a\b/c)",      V({L"a", L"b", L"c"}));
  CHECK(LR"(\\a\b//c)",    V({L"a", L"b", L"c"}));
  CHECK(LR"(\/\a\b//\c)",  V({L"a", L"b", L"c"}));

#undef CHECK
}

TEST_F(DirectoryEntryTests, forEachPathComponentStop)
{
  using details::forEachPathComponent;

  // makes sure forEachPathComponent() stop correctly during iteration

  auto checkStop = [](auto&& path) {
    // do a first split
    std::vector<std::wstring> components;
    forEachPathComponent(path, [&](std::wstring_view c, bool) {
      components.push_back(std::wstring(c.begin(), c.end()));
      return true;
    });


    // now that the actual number of components is known, call
    // forEachPathComponent() repeatedly and stop the iteration one further
    // each time

    for (std::size_t i=0; i<components.size(); ++i) {
      // will stop after this number of calls
      const std::size_t stopAfter = (i + 1);

      SCOPED_TRACE(L"stop after " + std::to_wstring(stopAfter));

      // actual number of calls
      std::size_t called = 0;

      forEachPathComponent(path, [&](std::wstring_view c, bool) {
        ++called;
        return (called < stopAfter);
      });

      EXPECT_EQ(called, stopAfter);
    }
  };

#define CHECK_STOP(PATH) \
  { \
    SCOPED_TRACE(PATH); \
    checkStop(PATH); \
  }

  CHECK_STOP(L"/");
  CHECK_STOP(L"//");
  CHECK_STOP(L"a");
  CHECK_STOP(L"a/");
  CHECK_STOP(L"a/b");
  CHECK_STOP(L"/a/b");
  CHECK_STOP(L"/a/b/");
  CHECK_STOP(L"/a/b/c");

#undef CHECK_STOP
}

TEST_F(DirectoryEntryTests, findSubDirectoryRecursive)
{
  createHierarchy();

  // tests path, path\ and path/
  //
  auto check = [&](auto&& from, std::wstring path, auto&& what) {
    checkCaseVariations(from, path, what);
    checkCaseVariations(from, path + L"\\", what);
    checkCaseVariations(from, path + L"/", what);
  };

#define CHECK(FROM, PATH, WHAT) \
  { \
    SCOPED_TRACE(FROM->debugName() + L" with path '" + PATH + L"'"); \
    check(FROM, PATH, WHAT); \
  }


  // non-existing
  EXPECT_EQ(root->findSubDirectoryRecursive(L"non-existing"), nullptr);

  // empty path returns the parent, check a couple of them
  CHECK(root.get(), L"", root.get());
  CHECK(meshes,     L"", meshes);
  CHECK(terrain,    L"", terrain);

  // from root
  CHECK(root.get(), L"meshes",                        meshes);
  CHECK(root.get(), L"meshes/non-existing",           nullptr);
  CHECK(root.get(), L"meshes/effects",                effects);
  CHECK(root.get(), L"meshes/landscape",              landscape);
  CHECK(root.get(), L"meshes/landscape/non-existing", nullptr);
  CHECK(root.get(), L"meshes/effects/blood.nif",      nullptr); // this is a file

  // from textures
  CHECK(textures, L"clutter",                       clutter);
  CHECK(textures, L"clutter/barrel01.dds",          nullptr); // this is a file
  CHECK(textures, L"terrain",                       terrain);
  CHECK(textures, L"terrain/noise.dds",             nullptr); // this is a file

#undef CHECK
}

TEST_F(DirectoryEntryTests, findFileRecursive)
{
  createHierarchy();

  // tests path, path\ and path/
  //
  auto check = [&](auto&& from, std::wstring path, auto&& what) {
    checkCaseVariations(from, path, FileEntryPtr(what));

    // trailing separator won't find anything
    checkCaseVariations(from, path + L"\\", FileEntryPtr());
    checkCaseVariations(from, path + L"/", FileEntryPtr());
  };

#define CHECK(FROM, PATH, WHAT) \
  { \
    SCOPED_TRACE(FROM->debugName() + L" with path '" + PATH + L"'"); \
    check(FROM, PATH, WHAT); \
  }


  // non-existing
  EXPECT_EQ(root->findFileRecursive(L"non-existing"), nullptr);

  // empty path won't find anything, check a couple
  CHECK(root.get(), L"", nullptr);
  CHECK(meshes,     L"", nullptr);
  CHECK(terrain,    L"", nullptr);

  // from root
  CHECK(root.get(), L"meshes",                    nullptr);    // this is a dir
  CHECK(root.get(), L"meshes/non-existing",       nullptr);    // this is a dir
  CHECK(root.get(), L"meshes/landscape",          nullptr);    // this is a dir
  CHECK(root.get(), L"meshes/landscape/tree",     nullptr);    // this is a dir
  CHECK(root.get(), L"meshes/landscape/tree/tree01.nif", tree01_nif);
  CHECK(root.get(), L"meshes/landscape/tree/tree02.nif", tree02_nif);

  // from textures
  CHECK(textures, L"clutter",                     nullptr);
  CHECK(textures, L"clutter/barrel01.dds",        barrel01_dds);
  CHECK(textures, L"terrain",                     nullptr);
  CHECK(textures, L"terrain/noise.dds",           noise_dds);

  // from terrain
  CHECK(terrain, L"noise.dds",                    noise_dds);

#undef CHECK
}
} // namespace tests
