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

#ifndef MO_REGISTER_DIRECTORYREFRESHER_INCLUDED
#define MO_REGISTER_DIRECTORYREFRESHER_INCLUDED

#include "fileregisterfwd.h"
#include "profile.h"
#include "envfs.h"
#include <QObject>
#include <thread>


// refresh progress, passed to the main window in the progress event
//
class DirectoryRefreshProgress : QObject
{
  Q_OBJECT;

public:
  using Callback = std::function<void (DirectoryRefreshProgress)>;

  DirectoryRefreshProgress(Callback cb={});

  DirectoryRefreshProgress(const DirectoryRefreshProgress& p);
  DirectoryRefreshProgress& operator=(const DirectoryRefreshProgress& p);

  // resets with the given total
  //
  void start(std::size_t total);


  // whether refreshing is finished
  //
  bool finished() const;

  // [0, 100]
  //
  int percentDone() const;


  // marks the progress as being finished
  //
  void finish();

  // adds one to the done count
  //
  void addDone();

private:
  Callback m_callback;
  std::size_t m_total;
  std::atomic<std::size_t> m_done;
  std::atomic<bool> m_finished;

  void notify();
};

Q_DECLARE_METATYPE(DirectoryRefreshProgress);


// used to asynchronously walk the mod directories and build the directory
// structure
//
class DirectoryStructure
{
  friend class DirectoryRefreshProgress;

public:
  using Progress = DirectoryRefreshProgress;
  using ProgressCallback = Progress::Callback;

  DirectoryStructure(std::size_t threadCount);
  ~DirectoryStructure();

  // non-copyable
  DirectoryStructure(const DirectoryStructure&) = delete;
  DirectoryStructure& operator=(const DirectoryStructure&) = delete;

  MOShared::DirectoryEntry* root();

  // add files for a mod to the directory structure, including bsas
  //
  void addMods(const std::vector<Profile::ActiveMod>& mods);

  // add only the bsas of a mod to the directory structure
  //
  void addBSAs(const std::vector<Profile::ActiveMod>& mods);

  // add only regular files or a mod to the directory structure
  //
  void addFiles(const std::vector<Profile::ActiveMod>& mods);

  DirectoryRefreshProgress progress() const;

  // generate a new directory structure
  //
  void asyncRefresh(
    const std::vector<Profile::ActiveMod>& mods,
    ProgressCallback callback);

private:
  struct ModThread
  {
    MOShared::DirectoryEntry* root = nullptr;
    DirectoryStructure* structure = nullptr;
    Progress* progress = nullptr;
    Profile::ActiveMod m;
    MOShared::DirectoryStats* stats =  nullptr;
    bool files = false;
    bool bsas = false;

    env::DirectoryWalker walker;
    std::condition_variable cv;
    std::mutex mutex;
    bool ready = false;

    void wakeup();
    void run();
  };

  std::unique_ptr<MOShared::DirectoryEntry> m_root;
  std::mutex m_refreshLock;
  std::size_t m_threadCount;
  std::thread m_thread;
  std::thread m_deleter;
  env::ThreadPool<ModThread> m_modThreads;
  Progress m_progress;

  void refreshThread(
    const std::vector<Profile::ActiveMod>& mods,
    ProgressCallback callback);

  void addMods(
    MOShared::DirectoryEntry* root,
    const std::vector<Profile::ActiveMod>& mods, bool files, bool bsas,
    Progress& p);

  void stealFiles(MOShared::DirectoryEntry* root, const Profile::ActiveMod& m);

  void addFiles(
    MOShared::DirectoryEntry* root,
    env::DirectoryWalker& walker, MOShared::DirectoryStats& stats,
    const Profile::ActiveMod& m);

  void addBSAs(
    MOShared::DirectoryEntry* root,
    MOShared::DirectoryStats& stats, const Profile::ActiveMod& m);
};

#endif // MO_REGISTER_DIRECTORYREFRESHER_INCLUDED
