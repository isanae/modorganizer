#ifndef MODINFOSEPARATOR_H
#define MODINFOSEPARATOR_H

#include "modinforegular.h"

class ModInfoSeparator : public ModInfoRegular
{
  Q_OBJECT;
  friend class ModInfo;

public:
  virtual bool updateAvailable() const { return false; }
  virtual bool updateIgnored() const { return false; }
  virtual bool downgradeAvailable() const { return false; }
  virtual bool updateNXMInfo() { return false; }
  virtual bool isValid() const { return true; }
  //TODO: Fix renaming method to avoid priority reset
  virtual bool setName(const QString& name);

  virtual int getNexusID() const { return -1; }

  virtual void setGameName(const QString& /*gameName*/) {}

  virtual void setNexusID(int /*modID*/) {}

  virtual void endorse(bool /*doEndorse*/) {}

  virtual void parseNexusInfo() {}

  virtual void ignoreUpdate(bool /*ignore*/) {}

  virtual bool canBeUpdated() const { return false; }
  virtual QDateTime getExpires() const { return QDateTime(); }
  virtual bool canBeEnabled() const { return false; }
  virtual std::vector<QString> getIniTweaks() const { return std::vector<QString>(); }

  virtual std::vector<EFlag> getFlags() const;
  virtual int getHighlight() const;

  virtual QString getDescription() const;
  virtual QString name() const;
  virtual QString getGameName() const { return ""; }
  virtual QString getInstallationFile() const { return ""; }
  virtual QString getURL() const { return ""; }
  virtual QString repository() const { return ""; }
  virtual int getNexusFileStatus() const { return 0; }
  virtual void setNexusFileStatus(int) {}
  virtual QDateTime getLastNexusUpdate() const { return QDateTime(); }
  virtual void setLastNexusUpdate(QDateTime) {}
  virtual QDateTime getLastNexusQuery() const { return QDateTime(); }
  virtual void setLastNexusQuery(QDateTime) {}
  virtual QDateTime getNexusLastModified() const { return QDateTime(); }
  virtual void setNexusLastModified(QDateTime) {}
  virtual QDateTime creationTime() const { return QDateTime(); }

  virtual void getNexusFiles
    (
      QList<MOBase::ModRepositoryFileInfo*>::const_iterator& /*unused*/,
      QList<MOBase::ModRepositoryFileInfo*>::const_iterator& /*unused*/)
  {
  }

  virtual QString getNexusDescription() const { return QString(); }

  virtual void addInstalledFile(int /*modId*/, int /*fileId*/)
  {
  }

private:
  ModInfoSeparator(
    PluginContainer* pluginContainer, const MOBase::IPluginGame* game,
    const QDir& path, OrganizerCore& core);
};

#endif
