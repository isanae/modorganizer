#ifndef MODINFOFOREIGN_H
#define MODINFOFOREIGN_H

#include "modinfowithconflictinfo.h"

class ModInfoForeign : public ModInfoWithConflictInfo
{
  Q_OBJECT;
  friend class ModInfo;

public:
  virtual bool updateAvailable() const { return false; }
  virtual bool updateIgnored() const { return false; }
  virtual bool downgradeAvailable() const { return false; }
  virtual bool updateNXMInfo() { return false; }
  virtual void setCategory(int, bool) {}
  virtual bool setName(const QString&) { return false; }
  virtual void setComments(const QString&) {}
  virtual void setNotes(const QString&) {}
  virtual void setGameName(const QString&) {}
  virtual void setNexusID(int) {}
  virtual void setNewestVersion(const MOBase::VersionInfo&) {}
  virtual void ignoreUpdate(bool) {}
  virtual void setNexusDescription(const QString&) {}
  virtual void setInstallationFile(const QString&) {}
  virtual void addNexusCategory(int) {}
  virtual void setIsEndorsed(bool) {}
  virtual void setNeverEndorse() {}
  virtual void setIsTracked(bool) {}
  virtual bool remove() { return false; }
  virtual void endorse(bool) {}
  virtual void track(bool) {}
  virtual void parseNexusInfo() {}
  virtual bool isEmpty() const { return false; }
  virtual QString name() const { return m_Name; }
  virtual QString internalName() const { return m_InternalName; }
  virtual QString comments() const { return ""; }
  virtual QString notes() const { return ""; }
  virtual QDateTime creationTime() const;
  virtual QString absolutePath() const;
  virtual MOBase::VersionInfo getNewestVersion() const { return QString(); }
  virtual QString getInstallationFile() const { return ""; }
  virtual QString getGameName() const { return ""; }
  virtual int getNexusID() const { return -1; }
  virtual QDateTime getExpires() const { return QDateTime(); }
  virtual std::vector<QString> getIniTweaks() const { return std::vector<QString>(); }
  virtual std::vector<ModInfo::EFlag> getFlags() const;
  virtual int getHighlight() const;
  virtual QString getDescription() const;
  virtual int getNexusFileStatus() const { return 0; }
  virtual void setNexusFileStatus(int) {}
  virtual QDateTime getLastNexusUpdate() const { return QDateTime(); }
  virtual void setLastNexusUpdate(QDateTime) {}
  virtual QDateTime getLastNexusQuery() const { return QDateTime(); }
  virtual void setLastNexusQuery(QDateTime) {}
  virtual QDateTime getNexusLastModified() const { return QDateTime(); }
  virtual void setNexusLastModified(QDateTime) {}
  virtual QString getNexusDescription() const { return QString(); }
  virtual int getFixedPriority() const { return INT_MIN; }
  virtual QStringList archives(bool checkOnDisk = false)  { return m_Archives; }
  virtual bool alwaysEnabled() const { return true; }
  virtual void addInstalledFile(int, int) {}

  // unmanaged ("foreign") mods are basically any esp, esl or esm files living
  // in the Data directory; each file found creates a pseudo-mod with the
  // filename as a name
  //
  // (note that some files are considered "official" if they match a
  // plugin-specific list of built in files, like "skyrim.esm" or
  // "dragonborn.esm")
  //
  // since they live in the Data directory, these files wouldn't typically be
  // associated with any mod (a mod is a directory in the `mods/` folder), they
  // would merely be shown in the Data tab as "unmanaged" and wouldn't be
  // present in the mod list at all
  //
  // so each esp, esl or esm file creates a "foreign" mod with the file as the
  // "reference" file and any .bsa with the same name will also be associated
  // with  it (plugins may add more files, but that's typically it)
  //
  // therefore, this function returns the list of files associated with this
  // foreign mod and is used by DirectoryStructure to change the origin of
  // unmanaged  files from "Data" to the appropriate foreign pseudo-mod
  //
  QStringList associatedFiles() const override
  {
    return m_Archives + QStringList(m_ReferenceFile);
  }

protected:
  ModInfoForeign(
    const QString &modName, const QString &referenceFile,
    const QStringList &archives, ModInfo::EModType modType,
    OrganizerCore& core, PluginContainer *pluginContainer);

private:
  QString m_Name;
  QString m_InternalName;
  QString m_ReferenceFile;
  QStringList m_Archives;
  QDateTime m_CreationTime;
  int m_Priority;
};

#endif // MODINFOFOREIGN_H
