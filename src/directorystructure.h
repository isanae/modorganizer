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

#ifndef MO_REGISTER_DIRECTORYSTRUCTURE_INCLUDED
#define MO_REGISTER_DIRECTORYSTRUCTURE_INCLUDED

#include "fileregisterfwd.h"
#include "envfs.h"
#include <QObject>
#include <thread>

struct ProfileActiveMod;

// refresh progress, passed to the main window in the progress event
//
class DirectoryRefreshProgress : QObject
{
  Q_OBJECT;

public:
  using Callback = std::function<void (DirectoryRefreshProgress)>;

  DirectoryRefreshProgress();
  DirectoryRefreshProgress(Callback cb, std::size_t total);

  DirectoryRefreshProgress(const DirectoryRefreshProgress& p);
  DirectoryRefreshProgress& operator=(const DirectoryRefreshProgress& p);


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
  mutable std::mutex m_mutex;
  Callback m_callback;
  std::size_t m_total;
  std::size_t m_done;
  bool m_finished;

  void notify();
};

Q_DECLARE_METATYPE(DirectoryRefreshProgress);


// holds the root DirectoryEntry and can add mods synchronously or do a full
// async refresh
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

  DirectoryEntry* root();


  // sets whether archive parsing is enabled; this merely sets a flag, the
  // structure needs to be refreshed
  //
  void setArchiveParsing(bool b);

  // convenience: forwards to OriginConnection::exists()
  //
  bool originExists(std::wstring_view name) const;

  // convenience: forwards to OriginConnection::findByID()
  //
  FilesOrigin* findOriginByID(OriginID id);
  const FilesOrigin* findOriginByID(OriginID id) const;

  // convenience: forwards to OriginConnection::findByName()
  //
  FilesOrigin* findOriginByName(std::wstring_view name);
  const FilesOrigin* findOriginByName(std::wstring_view name) const;

  // global register
  //
  std::shared_ptr<FileRegister> getFileRegister() const;


  // add files for a mod to the directory structure, including bsas; async,
  // but runs threads in the background
  //
  void addMods(const std::vector<ProfileActiveMod>& mods);

  // add only the bsas of a mod to the directory structure; async, but runs
  // threads in the background
  //
  void addBSAs(const std::vector<ProfileActiveMod>& mods);

  // add only regular files or a mod to the directory structure; async, but
  // runs threads in the background
  //
  void addFiles(const std::vector<ProfileActiveMod>& mods);

  void updateFiles(const std::vector<ProfileActiveMod>& mods);

  // returns the progress of a current async refresh, finished() is true if
  // there is no refresh running
  //
  DirectoryRefreshProgress progress() const;

  // starts a thread which calls addMods() and returns immediately, invokes
  // callback() regularly
  //
  void asyncRefresh(
    const std::vector<ProfileActiveMod>& mods,
    ProgressCallback callback);

private:
  // map of lowercase plugin names without extensions and load order index
  using LoadOrderMap = std::map<std::wstring, int, std::less<>>;

  // one refresh thread, calls functions into DirectoryStructure
  //
  // the only reason for keeping them around in the ThreadPool below is to
  // avoid destroying the DirectoryWalkers, which keep expensive buffers
  // around
  //
  class ModThread
  {
  public:
    ModThread();

    // sets up this thread
    //
    void set(
      DirectoryStructure* s, DirectoryEntry* root,
      ProfileActiveMod* m, Progress* p, bool addFiles, bool addBSAs);

    // runs it
    //
    void wakeup();

    // called from ThreadPool: waits until wakeup() is called
    void run();

  private:
    DirectoryStructure* m_structure;
    DirectoryEntry* m_root;
    ProfileActiveMod* m_mod;
    Progress* m_progress;
    bool m_addFiles;
    bool m_addBSAs;
    env::DirectoryWalker m_walker;
    env::Waiter m_waiter;
  };

  // root directory
  std::unique_ptr<DirectoryEntry> m_root;

  // file register
  std::shared_ptr<FileRegister> m_register;


  // locked between the varioud add*() functions and when swapping roots after
  // asyncRefresh() completes; this is probably mostly useless since the ui
  // won't allow multiple refreshes, and it's not locked in root() anyway, but
  // there it is
  std::mutex m_rootMutex;

  // thread count in async refresh
  std::size_t m_threadCount;

  // the main async refresh thread, starts all the other ones
  std::thread m_refreshThread;

  // the async refresh works on a temporary root item, which is swapped with
  // m_root when done; the old m_root is then deleted in this thread because
  // it can be pretty costly
  std::thread m_deleterThread;

  // refresh threads
  env::ThreadPool<ModThread> m_modThreads;

  // async refresh progress
  Progress m_progress;

  // whether archive parsing is enabled
  bool m_archiveParsing;


  // thread function called by asyncRefresh(): creates a new root, adds files
  // from Data and mods, sorts, then swaps m_root and spawns a deleter thread
  // for the old root
  //
  void refreshThread(
    const std::vector<ProfileActiveMod>& mods,
    ProgressCallback callback);

  // adds files from the data directory into the given root
  //
  void addFromData(DirectoryEntry* root);

  // starts a thread per mod in `mods`, up to `m_threadCount` active threads,
  // then adds files and bsas from it depending on the two bools
  //
  void addMods(
    DirectoryEntry* root, std::vector<ProfileActiveMod> mods,
    bool addFiles, bool addBSAs, Progress& p);

  // adds "associated files", typically files that are from a foreign mod;
  // see ModInfoForeign::associatedFiles() in modinfoforeign.h
  //
  void addAssociatedFiles(DirectoryEntry* root, const ProfileActiveMod& m);

  // adds files from the given mod's directory recursively
  //
  void addFiles(
    DirectoryEntry* root,
    env::DirectoryWalker& walker, const ProfileActiveMod& m);

  // adds files from all BSAs found in the given mod's directory
  //
  void addBSAs(DirectoryEntry* root, const ProfileActiveMod& m);

  // swaps the given register and root with the current ones and schedules root
  // for deletion in m_deleterThread
  //
  void setRoot(
    std::shared_ptr<FileRegister> fr,
    std::unique_ptr<DirectoryEntry> newRoot);

  // returns the plugin load order as a map of lowercase plugin names and
  // load index
  //
  LoadOrderMap getLoadOrderMap() const;

  // returns the index of the plugin associated with the given archive, or -1
  // if no associated plugin is found
  //
  int findArchiveLoadOrder(
    std::wstring_view archiveNameLc, const LoadOrderMap& loadOrderMap) const;
};

#endif // MO_REGISTER_DIRECTORYSTRUCTURE_INCLUDED
