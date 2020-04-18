#include "datatab.h"
#include "ui_mainwindow.h"
#include "settings.h"
#include "organizercore.h"
#include "messagedialog.h"
#include "filetree.h"
#include "filetreemodel.h"
#include "filetreeproviders.h"
#include <log.h>
#include <report.h>

using namespace MOShared;
using namespace MOBase;

// in mainwindow.cpp
QString UnmanagedModName();


DataTab::DataTab(OrganizerCore& core, QWidget* parent, Ui::MainWindow* mwui) :
  m_core(core), m_parent(parent),
  ui{
    mwui->dataTabRefresh, mwui->dataTree,
    mwui->dataTabShowOnlyConflicts, mwui->dataTabShowFromArchives},
  m_firstActivation(true)
{
  m_filetree.reset(new filetree::Tree(
    core, ui.tree, std::make_unique<filetree::VirtualProvider>(core)));

  m_filter.setUseSourceSort(true);
  m_filter.setFilterColumn(filetree::Model::FileName);
  m_filter.setEdit(mwui->dataTabFilter);
  m_filter.setList(mwui->dataTree);
  m_filter.setUpdateDelay(true);

  if (auto* m=m_filter.proxyModel()) {
    m->setDynamicSortFilter(false);
  }

  connect(
    &m_filter, &FilterWidget::aboutToChange,
    [&]{ ensureFullyLoaded(); });

  connect(
    ui.refresh, &QPushButton::clicked,
    [&]{ onRefresh(); });

  connect(
    ui.conflicts, &QCheckBox::toggled,
    [&]{ onConflicts(); });

  connect(
    ui.archives, &QCheckBox::toggled,
    [&]{ onArchives(); });

  connect(
    m_filetree.get(), &filetree::Tree::executablesChanged,
    this, &DataTab::executablesChanged);

  connect(
    m_filetree.get(), &filetree::Tree::originModified,
    this, &DataTab::originModified);

  connect(
    m_filetree.get(), &filetree::Tree::displayModInformation,
    this, &DataTab::displayModInformation);
}

void DataTab::saveState(Settings& s) const
{
  s.geometry().saveState(ui.tree->header());
  s.widgets().saveChecked(ui.conflicts);
  s.widgets().saveChecked(ui.archives);
}

void DataTab::restoreState(const Settings& s)
{
  s.geometry().restoreState(ui.tree->header());

  // prior to 2.3, the list was not sortable, and this remembered in the
  // widget state, for whatever reason
  ui.tree->setSortingEnabled(true);

  s.widgets().restoreChecked(ui.conflicts);
  s.widgets().restoreChecked(ui.archives);
}

void DataTab::activated()
{
  if (m_firstActivation) {
    m_firstActivation = false;
    updateTree();
  }
}

void DataTab::onRefresh()
{
  if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) {
    m_filetree->model()->setEnabled(false);
    m_filetree->clear();
  }

  m_core.refreshDirectoryStructure();
}

void DataTab::updateTree()
{
  m_filetree->model()->setEnabled(true);
  m_filetree->refresh();

  if (!m_filter.empty()) {
    ensureFullyLoaded();

    if (auto* m=m_filter.proxyModel()) {
      m->invalidate();
    }
  }
}

void DataTab::ensureFullyLoaded()
{
  if (!m_filetree->fullyLoaded()) {
    m_filter.proxyModel()->setRecursiveFilteringEnabled(false);
    m_filetree->ensureFullyLoaded();
    m_filter.proxyModel()->setRecursiveFilteringEnabled(true);
  }
}

void DataTab::onConflicts()
{
  updateOptions();
}

void DataTab::onArchives()
{
  updateOptions();
}

void DataTab::updateOptions()
{
  using M = filetree::Model;

  M::Flags flags = M::NoFlags;

  if (ui.conflicts->isChecked()) {
    flags |= M::ConflictsOnly | M::PruneDirectories;
  }

  if (ui.archives->isChecked()) {
    flags |= M::Archives;
  }

  m_filetree->model()->setFlags(flags);
  updateTree();
}
