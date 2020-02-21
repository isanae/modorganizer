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


DirectoryRefreshProgress::DirectoryRefreshProgress(DirectoryRefresher* r)
  : m_refresher(r), m_total(0), m_done(0), m_finished(false)
{
}

void DirectoryRefreshProgress::start(std::size_t total)
{
  m_total = total;
  m_done = 0;
  m_finished = false;

  m_refresher->updateProgress(this);
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
}

void DirectoryRefreshProgress::addDone()
{
  ++m_done;
  m_refresher->updateProgress(this);
}


void DirectoryRefresher::ModThread::wakeup()
{
  {
    std::scoped_lock lock(mutex);
    ready = true;
  }

  cv.notify_one();
}

void DirectoryRefresher::ModThread::run()
{
  std::unique_lock lock(mutex);
  cv.wait(lock, [&]{ return ready; });

  SetThisThreadName(m.mod->internalName() + " refresher");

  ds->addFromOrigin(
    {
      m.mod->internalName().toStdWString(),
      QDir::toNativeSeparators(m.mod->absolutePath()).toStdWString(),
      m.priority
    },
    walker, *stats);

  if (Settings::instance().archiveParsing()) {
    refresher->addModBSAToStructure(ds, m);
  }

  if (progress) {
    progress->addDone();
  }

  SetThisThreadName(QString::fromStdWString(L"idle refresher"));
  ready = false;
}


DirectoryRefresher::DirectoryRefresher(std::size_t threadCount)
  : m_threadCount(threadCount), m_lastFileCount(0), m_progress(this)
{
}

DirectoryRefresher::~DirectoryRefresher()
{
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

std::unique_ptr<DirectoryEntry> DirectoryRefresher::stealDirectoryStructure()
{
  std::scoped_lock lock(m_RefreshLock);
  return std::move(m_Root);
}

//void DirectoryRefresher::setMods(
//  const std::vector<std::tuple<QString, QString, int>> &mods,
//  const std::set<QString> &managedArchives)
//{
//  std::vector<ModEntry> v;
//
//  for (auto&& mod : mods) {
//    QString name = std::get<0>(mod);
//    QString path = std::get<1>(mod);
//    int prio = std::get<2>(mod);
//
//    ModInfo::Ptr info = ModInfo::getByIndex(ModInfo::getIndex(name));
//    v.push_back({name, path, info->stealFiles(), info->archives(), prio});
//  }
//
//  {
//    std::scoped_lock lock(m_RefreshLock);
//    m_Mods = std::move(v);
//    m_EnabledArchives = managedArchives;
//  }
//}

void DirectoryRefresher::addMultipleModsBSAToStructure(
  MOShared::DirectoryEntry *directoryStructure,
  const std::vector<Profile::ActiveMod>& mods)
{
  for (const auto& m : mods) {
    addModBSAToStructure(directoryStructure, m);
  }
}

void DirectoryRefresher::addModBSAToStructure(
  DirectoryEntry* root, const Profile::ActiveMod& m)
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

void DirectoryRefresher::stealModFilesIntoStructure(
  DirectoryEntry *directoryStructure, const Profile::ActiveMod& m)
{
  std::wstring directoryW = ToWString(QDir::toNativeSeparators(m.mod->absolutePath()));

  // instead of adding all the files of the target directory, we just change
  // the root of the specified files to this mod
  FilesOrigin& origin = directoryStructure->getOrCreateOrigin(
    {m.mod->internalName().toStdWString(), directoryW, m.priority});

  for (const QString &filename : m.mod->stealFiles()) {
    if (filename.isEmpty()) {
      log::warn("Trying to find file with no name");
      continue;
    }
    QFileInfo fileInfo(filename);
    FileEntryPtr file = directoryStructure->findFile(ToWString(fileInfo.fileName()));
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

void DirectoryRefresher::addModFilesToStructure(
  DirectoryEntry *directoryStructure, const Profile::ActiveMod& m)
{
  TimeThis tt("DirectoryRefresher::addModFilesToStructure()");

  std::wstring directoryW = ToWString(QDir::toNativeSeparators(m.mod->absolutePath()));
  DirectoryStats dummy;

  if (m.mod->stealFiles().length() > 0) {
    stealModFilesIntoStructure(directoryStructure, m);
  } else {
    directoryStructure->addFromOrigin(
      {m.mod->internalName().toStdWString(), directoryW, m.priority}, dummy);
  }
}

void DirectoryRefresher::addModToStructure(
  DirectoryEntry *directoryStructure, const Profile::ActiveMod& m)
{
  TimeThis tt("DirectoryRefresher::addModToStructure()");

  DirectoryStats dummy;

  if (!m.mod->stealFiles().empty()) {
    stealModFilesIntoStructure(directoryStructure, m);
  } else {
    std::wstring directoryW = ToWString(QDir::toNativeSeparators(m.mod->absolutePath()));
    directoryStructure->addFromOrigin(
      {m.mod->internalName().toStdWString(), directoryW, m.priority}, dummy);
  }

  if (Settings::instance().archiveParsing()) {
    addModBSAToStructure(directoryStructure, m);
  }
}

void DirectoryRefresher::updateProgress(const DirectoryRefreshProgress* p)
{
  // careful: called from multiple threads
  emit progress(p);
}

void DirectoryRefresher::requestProgressUpdate()
{
  if (!m_progress.finished()) {
    updateProgress(&m_progress);
  }
}

void DirectoryRefresher::addMultipleModsFilesToStructure(
  MOShared::DirectoryEntry *directoryStructure,
  const std::vector<Profile::ActiveMod>& mods,
  DirectoryRefreshProgress* progress)
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
      if (m.mod->stealFiles().length() > 0) {
        stealModFilesIntoStructure(directoryStructure, m);

        if (progress) {
          progress->addDone();
        }
      } else {
        auto& mt = m_modThreads.request();

        mt.refresher = this;
        mt.progress = progress;
        mt.ds = directoryStructure;
        mt.m = m;
        mt.stats = &stats[i];

        mt.wakeup();
      }
    } catch (const std::exception& ex) {
      emit error(tr("failed to read mod (%1): %2").arg(m.mod->internalName(), ex.what()));
    }
  }

  m_modThreads.waitForAll();

  if constexpr (DirectoryStats::EnableInstrumentation) {
    dumpStats(stats);
  }
}

void DirectoryRefresher::refreshThread(
  const std::vector<Profile::ActiveMod>& mods)
{
  SetThisThreadName("DirectoryRefresher");
  TimeThis tt("DirectoryRefresher::refresh()");

  {
    std::scoped_lock lock(m_RefreshLock);

    m_Root = DirectoryEntry::createRoot();

    IPluginGame *game = qApp->property("managed_game").value<IPluginGame*>();

    std::wstring dataDirectory =
      QDir::toNativeSeparators(game->dataDirectory().absolutePath()).toStdWString();

    {
      DirectoryStats dummy;
      m_Root->addFromOrigin({L"data", dataDirectory, 0}, dummy);
    }

    addMultipleModsFilesToStructure(m_Root.get(), mods, &m_progress);

    m_Root->getFileRegister()->sortOrigins();

    m_Root->cleanupIrrelevant();

    m_lastFileCount = m_Root->getFileRegister()->highestCount();
    log::debug("refresher saw {} files", m_lastFileCount);
  }

  m_progress.finish();

  emit progress(&m_progress);
  emit refreshed();
}

void DirectoryRefresher::asyncRefresh(
  const std::vector<Profile::ActiveMod>& mods)
{
  if (m_thread.joinable()) {
    m_thread.join();
  }

  m_progress.start(mods.size());
  m_thread = std::thread([this, mods]{ refreshThread(mods); });
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


