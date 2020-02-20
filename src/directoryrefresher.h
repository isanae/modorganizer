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
#include <QObject>
#include <thread>


class DirectoryRefreshProgress : QObject
{
  Q_OBJECT;

public:
  DirectoryRefreshProgress(DirectoryRefresher* r);

  void start(std::size_t modCount);

  bool finished() const;
  int percentDone() const;

  void finish();
  void addDone();

private:
  DirectoryRefresher* m_refresher;
  std::size_t m_modCount;
  std::atomic<std::size_t> m_modDone;
  std::atomic<bool> m_finished;
};


// used to asynchronously generate the virtual view of the combined data
// directory
//
class DirectoryRefresher : public QObject
{
  Q_OBJECT;
  friend class DirectoryRefreshProgress;

public:
  struct ModEntry
  {
    QString modName;
    QString absolutePath;
    QStringList stealFiles;
    QStringList archives;
    int priority;

    ModEntry(
      QString modName, QString absolutePath,
      QStringList stealFiles, QStringList aarchives, int priority);
  };

  DirectoryRefresher(std::size_t threadCount);
  ~DirectoryRefresher();

  // non-copyable
  DirectoryRefresher(const DirectoryRefresher&) = delete;
  DirectoryRefresher& operator=(const DirectoryRefresher&) = delete;

  // steals the internal DirectoryEntry pointer
  //
  std::unique_ptr<MOShared::DirectoryEntry> stealDirectoryStructure();

  // sets up the mods to be included in the directory structure
  //
  void setMods(
    const std::vector<std::tuple<QString, QString, int> > &mods,
    const std::set<QString> &managedArchives);

  // add files for a mod to the directory structure, including bsas
  //
  void addModToStructure(
    MOShared::DirectoryEntry *directoryStructure, const QString &modName,
    int priority, const QString &directory, const QStringList &stealFiles,
    const QStringList &archives);

  // add only the bsas of a mod to the directory structure
  //
  void addModBSAToStructure(
    MOShared::DirectoryEntry *directoryStructure, const QString &modName,
    int priority, const QString &directory, const QStringList &archives);

  // add only regular files or a mod to the directory structure
  //
  void addModFilesToStructure(
    MOShared::DirectoryEntry *directoryStructure, const QString &modName,
    int priority, const QString &directory, const QStringList &stealFiles);

  void addMultipleModsFilesToStructure(
    MOShared::DirectoryEntry *directoryStructure,
    const std::vector<ModEntry>& entries,
    DirectoryRefreshProgress* progress=nullptr);

  void requestProgressUpdate();

  // generate a directory structure from the mods set earlier
  void asyncRefresh();

signals:
  void progress(const DirectoryRefreshProgress* p);
  void error(const QString &error);
  void refreshed();

private:
  std::vector<ModEntry> m_Mods;
  std::set<QString> m_EnabledArchives;
  std::unique_ptr<MOShared::DirectoryEntry> m_Root;
  QMutex m_RefreshLock;
  std::size_t m_threadCount;
  std::size_t m_lastFileCount;
  std::thread m_thread;
  DirectoryRefreshProgress m_progress;

  void refreshThread();

  void updateProgress(const DirectoryRefreshProgress* p);

  void stealModFilesIntoStructure(
    MOShared::DirectoryEntry *directoryStructure, const QString &modName,
    int priority, const QString &directory, const QStringList &stealFiles);
};

#endif // MO_REGISTER_DIRECTORYREFRESHER_INCLUDED
