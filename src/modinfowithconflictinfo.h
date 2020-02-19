#ifndef MODINFOWITHCONFLICTINFO_H
#define MODINFOWITHCONFLICTINFO_H

#include "modinfo.h"

#include <QTime>

class ModInfoWithConflictInfo : public ModInfo
{
public:
  ModInfoWithConflictInfo(PluginContainer *pluginContainer, OrganizerCore& core);

  std::vector<ModInfo::EConflictFlag> getConflictFlags() const;
  virtual std::vector<ModInfo::EFlag> getFlags() const;

  /**
   * @brief clear all caches held for this mod
   */
  virtual void clearCaches();

  virtual std::set<unsigned int> getModOverwrite() { return m_OverwriteList; }

  virtual std::set<unsigned int> getModOverwritten() { return m_OverwrittenList; }

  virtual std::set<unsigned int> getModArchiveOverwrite() { return m_ArchiveOverwriteList; }

  virtual std::set<unsigned int> getModArchiveOverwritten() { return m_ArchiveOverwrittenList; }

  virtual std::set<unsigned int> getModArchiveLooseOverwrite() { return m_ArchiveLooseOverwriteList; }

  virtual std::set<unsigned int> getModArchiveLooseOverwritten() { return m_ArchiveLooseOverwrittenList; }

  virtual void doConflictCheck() const;

private:

  enum EConflictType {
    CONFLICT_NONE,
    CONFLICT_OVERWRITE,
    CONFLICT_OVERWRITTEN,
    CONFLICT_MIXED,
    CONFLICT_REDUNDANT,
    CONFLICT_CROSS
  };

private:

  /**
   * @return true if there is a conflict for files in this mod
   */
  EConflictType isConflicted() const;

  /**
   * @return true if there are archive conflicts for files in this mod
   */
  EConflictType isArchiveConflicted() const;

  /**
   * @return true if there are archive conflicts with loose files in this mod
   */
  EConflictType isLooseArchiveConflicted() const;

  /**
   * @return true if this mod is completely replaced by others
   */
  bool isRedundant() const;

  bool hasHiddenFiles() const;

private:
  OrganizerCore& m_Core;

  mutable EConflictType m_CurrentConflictState;
  mutable EConflictType m_ArchiveConflictState;
  mutable EConflictType m_ArchiveConflictLooseState;
  mutable bool m_HasLooseOverwrite;
  mutable bool m_HasHiddenFiles;
  mutable QTime m_LastConflictCheck;

  mutable std::set<unsigned int> m_OverwriteList;   // indices of mods overritten by this mod
  mutable std::set<unsigned int> m_OverwrittenList; // indices of mods overwriting this mod
  mutable std::set<unsigned int> m_ArchiveOverwriteList;   // indices of mods with archive files overritten by this mod
  mutable std::set<unsigned int> m_ArchiveOverwrittenList; // indices of mods with archive files overwriting this mod
  mutable std::set<unsigned int> m_ArchiveLooseOverwriteList; // indices of mods with archives being overwritten by this mod's loose files
  mutable std::set<unsigned int> m_ArchiveLooseOverwrittenList; // indices of mods with loose files overwriting this mod's archive files
};

#endif // MODINFOWITHCONFLICTINFO_H
