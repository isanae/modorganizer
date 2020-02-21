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


DirectoryRefreshProgress::DirectoryRefreshProgress(Callback cb)
  : m_callback(std::move(cb)), m_total(0), m_done(0), m_finished(false)
{
}

DirectoryRefreshProgress::DirectoryRefreshProgress(const DirectoryRefreshProgress& p)
{
  *this = p;
}

DirectoryRefreshProgress& DirectoryRefreshProgress::operator=(const DirectoryRefreshProgress& p)
{
  m_callback = p.m_callback;
  m_total = p.m_total;
  m_done = p.m_done.load();
  m_finished = p.m_finished.load();
  return *this;
}

void DirectoryRefreshProgress::start(std::size_t total)
{
  m_total = total;
  m_done = 0;
  m_finished = false;
  notify();
}

bool DirectoryRefreshProgress::finished() const
{
  return m_finished;
}

int DirectoryRefreshProgress::percentDone() const
{
  int percent = 100;

  if (m_total > 0) {
    const double d = static_cast<double>(m_done) / m_total;
    percent = static_cast<int>(d * 100);
  }

  return percent;
}

void DirectoryRefreshProgress::finish()
{
  m_finished = true;
  notify();
}

void DirectoryRefreshProgress::addDone()
{
  ++m_done;
  notify();
}

void DirectoryRefreshProgress::notify()
{
  if (m_callback) {
    m_callback(*this);
  }
}


void DirectoryStructure::ModThread::wakeup()
{
  {
    std::scoped_lock lock(mutex);
    ready = true;
  }

  cv.notify_one();
}

void DirectoryStructure::ModThread::run()
{
  std::unique_lock lock(mutex);
  cv.wait(lock, [&]{ return ready; });

  SetThisThreadName(m.mod->internalName() + " refresher");

  if (files) {
    structure->addFiles(root, walker, *stats, m);
  }

  if (bsas) {
    if (!m.mod->stealFiles().empty()) {
      structure->stealFiles(root, m);
    } else {
      structure->addFiles(root, walker, *stats, m);
    }
  }

  progress->addDone();

  SetThisThreadName(QString::fromStdWString(L"idle refresher"));
  ready = false;
}


DirectoryStructure::DirectoryStructure(std::size_t threadCount)
  : m_root(DirectoryEntry::createRoot()), m_threadCount(threadCount)
{
}

DirectoryStructure::~DirectoryStructure()
{
  if (m_thread.joinable()) {
    m_thread.join();
  }

  if (m_deleter.joinable()) {
    m_deleter.join();
  }
}

DirectoryEntry* DirectoryStructure::root()
{
  return m_root.get();
}

void DirectoryStructure::addMods(const std::vector<Profile::ActiveMod>& mods)
{
  Progress p;
  addMods(m_root.get(), mods, true, true, p);
}

void DirectoryStructure::addBSAs(const std::vector<Profile::ActiveMod>& mods)
{
  Progress p;
  addMods(m_root.get(), mods, false, true, p);
}

void DirectoryStructure::addFiles(const std::vector<Profile::ActiveMod>& mods)
{
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
  if (m_thread.joinable()) {
    m_thread.join();
  }

  m_thread = std::thread([=]{ refreshThread(mods, callback); });
}

void DirectoryStructure::addMods(
  DirectoryEntry *root,
  const std::vector<Profile::ActiveMod>& mods, bool files, bool bsas,
  Progress& p)
{
  std::vector<DirectoryStats> stats(mods.size());
  log::debug("refresher: using {} threads", m_threadCount);

  m_modThreads.setMax(m_threadCount);

  for (std::size_t i=0; i<mods.size(); ++i) {
    const auto& m = mods[i];
    const int prio = static_cast<int>(i + 1);

    if constexpr (DirectoryStats::EnableInstrumentation) {
      stats[i].mod = m.mod->internalName().toStdString();
    }

    try
    {
      auto& mt = m_modThreads.request();

      mt.root = root;
      mt.structure = this;
      mt.progress = &p;
      mt.m = m;
      mt.stats = &stats[i];
      mt.files = files;
      mt.bsas = bsas;

      mt.wakeup();
    } catch (const std::exception& e) {
      log::error("failed to read mod {}: {}", m.mod->internalName(), e.what());
    }
  }

  m_modThreads.waitForAll();

  if constexpr (DirectoryStats::EnableInstrumentation) {
    dumpStats(stats);
  }
}

void DirectoryStructure::stealFiles(DirectoryEntry* root, const Profile::ActiveMod& m)
{
  std::wstring directoryW = ToWString(QDir::toNativeSeparators(m.mod->absolutePath()));

  // instead of adding all the files of the target directory, we just change
  // the root of the specified files to this mod
  FilesOrigin& origin = root->getOrCreateOrigin(
    {m.mod->internalName().toStdWString(), directoryW, m.priority});

  for (const QString &filename : m.mod->stealFiles()) {
    if (filename.isEmpty()) {
      log::warn("Trying to find file with no name");
      continue;
    }
    QFileInfo fileInfo(filename);
    FileEntryPtr file = root->findFile(ToWString(fileInfo.fileName()));
    if (file.get() != nullptr) {
      if (file->getOrigin() == 0) {
        // replace data as the origin on this bsa
        file->removeOrigin(0);
      }
      origin.addFile(file->getIndex());
      file->addOrigin(origin.getID(), file->getFileTime(), L"", -1);
    } else {
      QString warnStr = fileInfo.absolutePath();
      if (warnStr.isEmpty())
        warnStr = filename;
      log::warn("file not found: {}", warnStr);
    }
  }
}

void DirectoryStructure::addFiles(
  DirectoryEntry* root, env::DirectoryWalker& walker, MOShared::DirectoryStats& stats,
  const Profile::ActiveMod& m)
{
  //TimeThis tt("DirectoryStructure::addModFilesToStructure()");

  std::wstring directoryW = ToWString(QDir::toNativeSeparators(m.mod->absolutePath()));
  DirectoryStats dummy;

  root->addFromOrigin(
    {m.mod->internalName().toStdWString(), directoryW, m.priority}, dummy);
}

void DirectoryStructure::addBSAs(
  DirectoryEntry* root,
  MOShared::DirectoryStats& stats, const Profile::ActiveMod& m)
{
  const IPluginGame *game =
    qApp->property("managed_game").value<IPluginGame*>();

  GamePlugins *gamePlugins = game->feature<GamePlugins>();
  QStringList loadOrder = QStringList();
  gamePlugins->getLoadOrder(loadOrder);

  DirectoryStats dummy;

  for (const auto& archive : m.mod->archives()) {
    const std::filesystem::path archivePath(archive.toStdWString());
    const auto filename = QString::fromStdWString(archivePath.filename().native());
    const auto filenameLc = filename.toLower();

    int order = -1;

    for (auto plugin : loadOrder)
    {
      const auto pluginNameLc =
        ToLowerCopy(std::filesystem::path(plugin.toStdWString()).stem().native());

      if (filenameLc.startsWith(pluginNameLc + L" - ") ||
        filenameLc.startsWith(pluginNameLc + L".")) {
        auto itor = std::find(loadOrder.begin(), loadOrder.end(), plugin);
        if (itor != loadOrder.end()) {
          order = std::distance(loadOrder.begin(), itor);
        }
      }
    }

    root->addFromBSA(
      {
        m.mod->internalName().toStdWString(),
        QDir::toNativeSeparators(m.mod->absolutePath()).toStdWString(),
        m.priority
      },
      archivePath, order, dummy);
  }
}

void DirectoryStructure::refreshThread(
  const std::vector<Profile::ActiveMod>& mods,
  ProgressCallback callback)
{
  SetThisThreadName("DirectoryStructure");
  //TimeThis tt("DirectoryStructure::refresh()");

  m_progress = Progress(callback);

  {
    std::scoped_lock lock(m_refreshLock);
    auto root = DirectoryEntry::createRoot();

    IPluginGame *game = qApp->property("managed_game").value<IPluginGame*>();

    std::wstring dataDirectory =
      QDir::toNativeSeparators(game->dataDirectory().absolutePath()).toStdWString();

    {
      DirectoryStats dummy;
      root->addFromOrigin({L"data", dataDirectory, 0}, dummy);
    }

    addMods(root.get(), mods, true, true, m_progress);

    root->getFileRegister()->sortOrigins();
    root->cleanupIrrelevant();

    log::debug("refresher saw {} files", root->getFileRegister()->highestCount());

    // swapping with current
    std::swap(m_root, root);

    if (m_deleter.joinable()) {
      m_deleter.join();
    }

    m_deleter = std::thread([p=root.release()]{
      TimeThis tt("structure deleter");
      delete p;
    });
  }

  m_progress.finish();
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


