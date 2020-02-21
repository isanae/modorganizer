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
  DirectoryRefreshProgress(DirectoryRefresher* r);

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
  DirectoryRefresher* m_refresher;
  std::size_t m_total;
  std::atomic<std::size_t> m_done;
  std::atomic<bool> m_finished;
};


// used to asynchronously walk the mod directories and build the directory
// structure
//
class DirectoryRefresher : public QObject
{
  Q_OBJECT;
  friend class DirectoryRefreshProgress;

public:
  DirectoryRefresher(std::size_t threadCount);
  ~DirectoryRefresher();

  // non-copyable
  DirectoryRefresher(const DirectoryRefresher&) = delete;
  DirectoryRefresher& operator=(const DirectoryRefresher&) = delete;

  // steals the internal DirectoryEntry pointer
  //
  std::unique_ptr<MOShared::DirectoryEntry> stealDirectoryStructure();

  // add files for a mod to the directory structure, including bsas
  //
  void addModToStructure(
    MOShared::DirectoryEntry *directoryStructure,
    const Profile::ActiveMod& mod);

  // add only the bsas of a mod to the directory structure
  //
  void addModBSAToStructure(
    MOShared::DirectoryEntry *directoryStructure,
    const Profile::ActiveMod& mod);

  void addMultipleModsBSAToStructure(
    MOShared::DirectoryEntry *directoryStructure,
    const std::vector<Profile::ActiveMod>& mods);

  // add only regular files or a mod to the directory structure
  //
  void addModFilesToStructure(
    MOShared::DirectoryEntry *directoryStructure,
    const Profile::ActiveMod& entry);

  void addMultipleModsFilesToStructure(
    MOShared::DirectoryEntry *directoryStructure,
    const std::vector<Profile::ActiveMod>& mods,
    DirectoryRefreshProgress* progress=nullptr);

  void requestProgressUpdate();

  // generate a directory structure from the mods set earlier
  void asyncRefresh(const std::vector<Profile::ActiveMod>& mods);

signals:
  void progress(const DirectoryRefreshProgress* p);
  void error(const QString &error);
  void refreshed();

private:
  struct ModThread
  {
    DirectoryRefresher* refresher = nullptr;
    DirectoryRefreshProgress* progress = nullptr;
    MOShared::DirectoryEntry* ds = nullptr;
    Profile::ActiveMod m;
    MOShared::DirectoryStats* stats =  nullptr;

    env::DirectoryWalker walker;
    std::condition_variable cv;
    std::mutex mutex;
    bool ready = false;

    void wakeup();
    void run();
  };

  std::unique_ptr<MOShared::DirectoryEntry> m_Root;
  std::mutex m_RefreshLock;
  std::size_t m_threadCount;
  std::size_t m_lastFileCount;
  std::thread m_thread;
  env::ThreadPool<ModThread> m_modThreads;
  DirectoryRefreshProgress m_progress;

  void refreshThread(const std::vector<Profile::ActiveMod>& mods);

  void updateProgress(const DirectoryRefreshProgress* p);

  void stealModFilesIntoStructure(
    MOShared::DirectoryEntry *directoryStructure,
    const Profile::ActiveMod& entry);
};

#endif // MO_REGISTER_DIRECTORYREFRESHER_INCLUDED
