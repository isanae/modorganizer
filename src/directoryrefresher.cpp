/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "directoryrefresher.h"
#include "shared/fileentry.h"
#include "shared/filesorigin.h"
#include "shared/directoryentry.h"
#include "shared/originconnection.h"

#include "iplugingame.h"
#include "modinfo.h"
#include "settings.h"
#include "envfs.h"
#include "util.h"

#include <utility.h>
#include <gameplugins.h>

using namespace MOBase;
using namespace MOShared;

void dumpStats(std::vector<DirectoryStats>& stats);


DirectoryRefreshProgress::DirectoryRefreshProgress()
  : m_total(0), m_done(0), m_finished(false)
{
}

DirectoryRefreshProgress::DirectoryRefreshProgress(Callback cb, std::size_t t)
  : m_callback(std::move(cb)), m_total(t), m_done(0), m_finished(false)
{
  notify();
}

DirectoryRefreshProgress::DirectoryRefreshProgress(const DirectoryRefreshProgress& p)
{
  *this = p;
}

DirectoryRefreshProgress& DirectoryRefreshProgress::operator=(const DirectoryRefreshProgress& p)
{
  if (&p != this) {
    std::scoped_lock lock(m_mutex, p.m_mutex);

    m_callback = p.m_callback;
    m_total = p.m_total;
    m_done = p.m_done;
    m_finished = p.m_finished;
  }

  return *this;
}

bool DirectoryRefreshProgress::finished() const
{
  std::scoped_lock lock(m_mutex);
  return m_finished;
}

int DirectoryRefreshProgress::percentDone() const
{
  int percent = 100;

  {
    std::scoped_lock lock(m_mutex);

    if (m_total > 0) {
      const double d = static_cast<double>(m_done) / m_total;
      percent = static_cast<int>(d * 100);
    }
  }

  return percent;
}

void DirectoryRefreshProgress::finish()
{
  {
    std::scoped_lock lock(m_mutex);
    m_finished = true;
  }

  notify();
}

void DirectoryRefreshProgress::addDone()
{
  {
    std::scoped_lock lock(m_mutex);
    ++m_done;
  }

  notify();
}

void DirectoryRefreshProgress::notify()
{
  Callback cb;

  {
    std::scoped_lock lock(m_mutex);
    cb = m_callback;
  }

  if (cb) {
    cb(*this);
  }
}


DirectoryStructure::ModThread::ModThread() :
  m_structure(nullptr), m_root(nullptr), m_progress(nullptr), m_stats(nullptr),
  m_addFiles(false), m_addBSAs(false)
{
}

void DirectoryStructure::ModThread::set(
  DirectoryStructure* s, MOShared::DirectoryEntry* root,
  Profile::ActiveMod m, Progress* p, MOShared::DirectoryStats* stats,
  bool addFiles, bool addBSAs)
{
  m_structure = s;
  m_root = root;
  m_progress = p;
  m_mod = m;
  m_stats = stats;
  m_addFiles = addFiles;
  m_addBSAs = addBSAs;
}

void DirectoryStructure::ModThread::wakeup()
{
  m_waiter.wakeup();
}

void DirectoryStructure::ModThread::run()
{
  try
  {
      m_waiter.wait();

      SetThisThreadName(m_mod.mod->internalName() + " refresher");

      if (m_addFiles) {
        if (!m_mod.mod->associatedFiles().empty()) {
          m_structure->addAssociatedFiles(m_root, m_mod);
        } else {
          m_structure->addFiles(m_root, m_walker, *m_stats, m_mod);
        }
      }

      if (m_addBSAs) {
        m_structure->addBSAs(m_root, *m_stats, m_mod);
      }

      m_progress->addDone();

      SetThisThreadName(QString::fromStdWString(L"idle refresher"));
  }
  catch(std::exception& e)
  {
    log::error(
      "unhandled exception in ModThread for '{}': {}",
      m_mod.mod->internalName(), e.what());
  }
  catch(...)
  {
    log::error(
      "unhandled unknown exception in ModThread for '{}'",
      m_mod.mod->internalName());
  }
}


DirectoryStructure::DirectoryStructure(std::size_t threadCount)
  : m_threadCount(threadCount)
{
  log::debug("refresher is using {} threads", m_threadCount);

  m_register = FileRegister::create();
  m_root = DirectoryEntry::createRoot(m_register);
}

DirectoryStructure::~DirectoryStructure()
{
  if (m_refreshThread.joinable()) {
    m_refreshThread.join();
  }

  if (m_deleterThread.joinable()) {
    m_deleterThread.join();
  }
}

DirectoryEntry* DirectoryStructure::root()
{
  return m_root.get();
}

bool DirectoryStructure::originExists(std::wstring_view name) const
{
  return m_register->getOriginConnection()->exists(name);
}

FilesOrigin* DirectoryStructure::findOriginByID(OriginID id)
{
  return m_register->getOriginConnection()->findByID(id);
}

const FilesOrigin* DirectoryStructure::findOriginByID(OriginID id) const
{
  return m_register->getOriginConnection()->findByID(id);
}

FilesOrigin* DirectoryStructure::findOriginByName(std::wstring_view name)
{
  return m_register->getOriginConnection()->findByName(name);
}

const FilesOrigin* DirectoryStructure::findOriginByName(
  std::wstring_view name) const
{
  return m_register->getOriginConnection()->findByName(name);
}

std::shared_ptr<MOShared::FileRegister>
DirectoryStructure::getFileRegister() const
{
  return m_register;
}

void DirectoryStructure::addMods(const std::vector<Profile::ActiveMod>& mods)
{
  std::scoped_lock lock(m_rootMutex);
  TimeThis tt("DirectoryStructure::addMods()");

  Progress p;
  addMods(m_root.get(), mods, true, true, p);
}

void DirectoryStructure::addBSAs(const std::vector<Profile::ActiveMod>& mods)
{
  std::scoped_lock lock(m_rootMutex);
  TimeThis tt("DirectoryStructure::addBSAs()");

  Progress p;
  addMods(m_root.get(), mods, false, true, p);
}

void DirectoryStructure::addFiles(const std::vector<Profile::ActiveMod>& mods)
{
  std::scoped_lock lock(m_rootMutex);
  TimeThis tt("DirectoryStructure::addFiles()");

  Progress p;
  addMods(m_root.get(), mods, true, false, p);
}

DirectoryRefreshProgress DirectoryStructure::progress() const
{
  return m_progress;
}

void DirectoryStructure::asyncRefresh(
  const std::vector<Profile::ActiveMod>& mods, ProgressCallback callback)
{
  // make sure any previous refresh has finished
  if (m_refreshThread.joinable()) {
    m_refreshThread.join();
  }

  m_refreshThread = std::thread([=]{ refreshThread(mods, callback); });
}

void DirectoryStructure::addFromData(DirectoryEntry* root)
{
  const auto* game = qApp->property("managed_game").value<IPluginGame*>();
  const auto dir = game->dataDirectory().absolutePath();
  const auto dirW = QDir::toNativeSeparators(dir).toStdWString();

  DirectoryStats dummyStats;
  root->addFromOrigin({L"data", dirW, DataOriginID}, dummyStats);
}

void DirectoryStructure::addMods(
  DirectoryEntry* root,
  const std::vector<Profile::ActiveMod>& mods, bool addFiles, bool addBSAs,
  Progress& p)
{
  // one stats object per mod
  std::vector<DirectoryStats> stats(mods.size());

  // creating threads
  m_modThreads.setMax(m_threadCount);

  // for each mod
  for (std::size_t i=0; i<mods.size(); ++i) {
    const auto& m = mods[i];
    const int prio = static_cast<int>(i + 1);

    if constexpr (DirectoryStats::EnableInstrumentation) {
      stats[i].mod = m.mod->internalName().toStdString();
    }

    // request an idle thread
    auto& mt = m_modThreads.request();

    // set it up
    mt.set(this, root, m, &p, &stats[i], addFiles, addBSAs);

    // make it run
    mt.wakeup();
  }

  // for wait the remaining threads to finish
  m_modThreads.waitForAll();

  if constexpr (DirectoryStats::EnableInstrumentation) {
    dumpStats(stats);
  }
}

void DirectoryStructure::addAssociatedFiles(
  DirectoryEntry* root, const Profile::ActiveMod& m)
{
  // these files are already in the structure because they're actually in the
  // Data directory, and it's been checked at the very beginning
  //
  // note that some files might be both in Data and in a mod
  //
  // so there's no point in walking the filesystem for them, just change their
  // origin from Data to the given pseudo, foreign mod

  FilesOrigin& origin = root->getOrCreateOrigin({
    m.mod->internalName().toStdWString(),
    QDir::toNativeSeparators(m.mod->absolutePath()).toStdWString(),
    m.priority
  });

  for (const QString& filename : m.mod->associatedFiles()) {
    if (filename.isEmpty()) {
      log::error(
        "while adding associated files for mod '{}', "
        "a file had an empty filename", m.mod->internalName());

      continue;
    }

    const QFileInfo fileInfo(filename);
    const auto file = root->findFile(fileInfo.fileName().toStdWString());

    if (!file) {
      log::error(
        "while adding associated files for mod '{}', "
        "file '{}' was not found in the structure",
        m.mod->internalName(), fileInfo.fileName());

      continue;
    }

    // remove Data as an origin because it would put the same file in two
    // origins
    if (file->getOrigin() == DataOriginID) {
      file->removeOrigin(DataOriginID);
    }

    // add file to origin
    origin.addFile(file->getIndex());

    // add origin to file
    file->addOrigin({origin.getID(), {}}, file->getFileTime());
  }
}

void DirectoryStructure::addFiles(
  DirectoryEntry* root, env::DirectoryWalker& walker, MOShared::DirectoryStats& stats,
  const Profile::ActiveMod& m)
{
  root->addFromOrigin(
    {
      m.mod->internalName().toStdWString(),
      QDir::toNativeSeparators(m.mod->absolutePath()).toStdWString(),
      m.priority
    },
    walker, stats);
}

void DirectoryStructure::addBSAs(
  DirectoryEntry* root,
  MOShared::DirectoryStats& stats, const Profile::ActiveMod& m)
{
  if (!Settings::instance().archiveParsing()) {
    // archive parsing is disabled
    return;
  }

  // getting load order
  const auto loadOrderMap = getLoadOrderMap();

  // for each archive in this mod
  for (const auto& archive : m.mod->archives()) {
    const fs::path archivePath(archive.toStdWString());

    // lowercase name without extension
    const auto archiveNameLc = ToLowerCopy(archivePath.stem().native());

    // look for an associated plugin in the load order
    const int order = findArchiveLoadOrder(archiveNameLc, loadOrderMap);

    if (order == -1) {
      log::warn(
        "while adding BSAs for mod '{}', archive '{}' has no "
        "corresponding plugin in the load order file",
        m.mod->internalName(), archive);
    }

    root->addFromBSA(
      {
        m.mod->internalName().toStdWString(),
        QDir::toNativeSeparators(m.mod->absolutePath()).toStdWString(),
        m.priority
      },
      archivePath, order, stats);
  }
}

DirectoryStructure::LoadOrderMap DirectoryStructure::getLoadOrderMap() const
{
  const auto* game = qApp->property("managed_game").value<IPluginGame*>();
  auto* gamePlugins = game->feature<GamePlugins>();

  // getting loadorder as a list of plugin file names
  QStringList loadOrder;
  gamePlugins->getLoadOrder(loadOrder);

  LoadOrderMap loadOrderMap;

  // for each plugin
  for (int i=0; i<loadOrder.size(); ++i) {
    const fs::path pluginPath(loadOrder[i].toStdWString());

    // lowercase plugin name without extension
    const std::wstring pluginNameLc = ToLowerCopy(pluginPath.stem().native());

    // add to map
    loadOrderMap.emplace(pluginNameLc, i);
  }

  return loadOrderMap;
}

int DirectoryStructure::findArchiveLoadOrder(
  std::wstring_view archiveNameLc, const LoadOrderMap& loadOrderMap) const
{
  // BSAs typically have the same filename as their associated plugin, so for
  // example "Bells of Skyrim.esp" and "Bells of Skyrim.bsa"
  //
  // but some mods have more than one BSA and those typically have " - X" at
  // the end, like "Bells of Skyrim - Textures.bsa"
  //
  // this first look for the archive filename without extension in the name;
  // if it's not found, it cuts the filename on the last " - " and looks for
  // that

  // looking for the archive name itself
  auto itor = loadOrderMap.find(archiveNameLc);
  if (itor != loadOrderMap.end()) {
    return itor->second;
  }

  // cut on the last " - "
  const auto pos = archiveNameLc.rfind(L" - ");
  if (pos != std::wstring::npos) {
    const auto archivePrefixLc = archiveNameLc.substr(0, pos);

    // look for that
    itor = loadOrderMap.find(archivePrefixLc);
    if (itor != loadOrderMap.end()) {
      return itor->second;
    }
  }

  // not found
  return -1;
}

void DirectoryStructure::setRoot(
  std::shared_ptr<FileRegister> fr,
  std::unique_ptr<DirectoryEntry> root)
{
  // swapping with current
  {
    std::scoped_lock lock(m_rootMutex);
    std::swap(m_register, fr);
    std::swap(m_root, root);
  }

  // fr and root now point to the old data

  // finishing previous deletion, if any
  if (m_deleterThread.joinable()) {
    m_deleterThread.join();
  }

  // deleting the old root structure in a thread
  m_deleterThread = std::thread([p=root.release()]{
    TimeThis tt("structure deleter");
    delete p;
  });
}

void DirectoryStructure::refreshThread(
  const std::vector<Profile::ActiveMod>& mods,
  ProgressCallback callback)
{
  SetThisThreadName("DirectoryStructure");
  TimeThis tt("DirectoryStructure::refreshThread()");

  try
  {
    // restart progress
    m_progress = Progress(callback, mods.size());

    {
      // new register and root, will be swapped when everything's done
      auto fr = FileRegister::create();
      auto root = DirectoryEntry::createRoot(fr);

      // add from data directory
      addFromData(root.get());

      // add mods
      addMods(root.get(), mods, true, true, m_progress);

      // cleanup
      root->getFileRegister()->sortOrigins();
      root->cleanupIrrelevant();

      // swapping
      setRoot(std::move(fr), std::move(root));

      // fr and root have been moved from this point
    }

    // final notification
    m_progress.finish();

    log::debug(
      "refresher saw {} files in {} mods",
      m_root->getFileRegister()->fileCount(), mods.size());
  }
  catch(std::exception& e)
  {
    log::error("unhandled exception in refreshThread: {}", e.what());
  }
  catch(...)
  {
    log::error("unhandled unknown exception in refreshThread");
  }
}


DirectoryStats::DirectoryStats()
{
  std::memset(this, 0, sizeof(DirectoryStats));
}

DirectoryStats& DirectoryStats::operator+=(const DirectoryStats& o)
{
  dirTimes += o.dirTimes;
  fileTimes += o.fileTimes;
  sortTimes += o.sortTimes;

  subdirLookupTimes += o.subdirLookupTimes;
  addDirectoryTimes += o.addDirectoryTimes;

  filesLookupTimes += o.filesLookupTimes;
  addFileTimes += o.addFileTimes;
  addOriginToFileTimes += o.addOriginToFileTimes;
  addFileToOriginTimes += o.addFileToOriginTimes;
  addFileToRegisterTimes += o.addFileToRegisterTimes;

  return *this;
}

std::string DirectoryStats::csvHeader()
{
  QStringList sl = {
    "dirTimes",
    "fileTimes",
    "sortTimes",
    "subdirLookupTimes",
    "addDirectoryTimes",
    "filesLookupTimes",
    "addFileTimes",
    "addOriginToFileTimes",
    "addFileToOriginTimes",
    "addFileToRegisterTimes"
  };

  return sl.join(",").toStdString();
}

std::string DirectoryStats::toCsv() const
{
  QStringList oss;

  auto s = [](auto ns) {
    return ns.count() / 1000.0 / 1000.0 / 1000.0;
  };

  oss
    << QString::number(s(dirTimes))
    << QString::number(s(fileTimes))
    << QString::number(s(sortTimes))

    << QString::number(s(subdirLookupTimes))
    << QString::number(s(addDirectoryTimes))

    << QString::number(s(filesLookupTimes))
    << QString::number(s(addFileTimes))
    << QString::number(s(addOriginToFileTimes))
    << QString::number(s(addFileToOriginTimes))
    << QString::number(s(addFileToRegisterTimes));

  return oss.join(",").toStdString();
}

void dumpStats(std::vector<DirectoryStats>& stats)
{
  static int run = 0;
  static const std::string file("c:\\tmp\\data.csv");

  if (run == 0) {
    std::ofstream out(file, std::ios::out|std::ios::trunc);
    out << fmt::format("what,run,{}", DirectoryStats::csvHeader()) << "\n";
  }

  std::sort(stats.begin(), stats.end(), [](auto&& a, auto&& b){
    return (naturalCompare(QString::fromStdString(a.mod), QString::fromStdString(b.mod)) < 0);
  });

  std::ofstream out(file, std::ios::app);

  DirectoryStats total;
  for (const auto& s : stats) {
    out << fmt::format("{},{},{}", s.mod, run, s.toCsv()) << "\n";
    total += s;
  }

  out << fmt::format("total,{},{}", run, total.toCsv()) << "\n";

  ++run;
}
