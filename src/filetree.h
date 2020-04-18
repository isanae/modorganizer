#ifndef MODORGANIZER_FILETREE_INCLUDED
#define MODORGANIZER_FILETREE_INCLUDED

#include "filetreefwd.h"
#include "modinfo.h"
#include "modinfodialogfwd.h"

namespace MOShared { class FileEntry; }

class OrganizerCore;

namespace filetree
{

class Tree : public QObject
{
  Q_OBJECT;

public:
  Tree(OrganizerCore& core, QTreeView* tree);

  Model* model();
  void refresh();
  void clear();

  bool fullyLoaded() const;
  void ensureFullyLoaded();

  void open(Item* item=nullptr);
  void openHooked(Item* item=nullptr);
  void preview(Item* item=nullptr);
  void activate(Item* item=nullptr);

  void addAsExecutable(Item* item=nullptr);
  void exploreOrigin(Item* item=nullptr);
  void openModInfo(Item* item=nullptr);

  void hide(Item* item=nullptr);
  void unhide(Item* item=nullptr);

  void dumpToFile() const;

signals:
  void executablesChanged();
  void originModified(int originID);
  void displayModInformation(ModInfo::Ptr m, unsigned int i, ModInfoTabIDs tab);

private:
  OrganizerCore& m_core;
  QTreeView* m_tree;
  Model* m_model;

  Item* singleSelection();

  void onExpandedChanged(const QModelIndex& index, bool expanded);
  void onItemActivated(const QModelIndex& index);
  void onContextMenu(const QPoint &pos);
  bool showShellMenu(QPoint pos);

  void addDirectoryMenus(QMenu& menu, Item& item);
  void addFileMenus(QMenu& menu, const MOShared::FileEntry& file, int originID);
  void addOpenMenus(QMenu& menu, const MOShared::FileEntry& file);
  void addCommonMenus(QMenu& menu);

  void toggleVisibility(bool b, Item* item=nullptr);

  QModelIndex proxiedIndex(const QModelIndex& index);
};

} // namespace

#endif // MODORGANIZER_FILETREE_INCLUDED
