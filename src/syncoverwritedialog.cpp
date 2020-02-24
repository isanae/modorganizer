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

#include "syncoverwritedialog.h"
#include "shared/directoryentry.h"
#include "shared/fileentry.h"
#include "shared/filesorigin.h"
#include "shared/originconnection.h"
#include "directoryrefresher.h"
#include "ui_syncoverwritedialog.h"

#include <utility.h>
#include <report.h>
#include <log.h>

#include <QDir>
#include <QDirIterator>
#include <QComboBox>
#include <QStringList>


using namespace MOBase;
using namespace MOShared;


SyncOverwriteDialog::SyncOverwriteDialog(
  const QString &path, DirectoryStructure* directoryStructure,
  QWidget *parent) :
    TutorableDialog("SyncOverwrite", parent),
    ui(new Ui::SyncOverwriteDialog),
    m_SourcePath(path),
    m_DirectoryStructure(directoryStructure)
{
  ui->setupUi(this);
  refresh(path);

  QHeaderView *headerView = ui->syncTree->header();
  headerView->setSectionResizeMode(0, QHeaderView::Stretch);
  headerView->setSectionResizeMode(1, QHeaderView::Interactive);
}

SyncOverwriteDialog::~SyncOverwriteDialog()
{
  delete ui;
}

static void addToComboBox(QComboBox *box, const QString &name, const QVariant &userData)
{
  if (QString::compare(name, "overwrite", Qt::CaseInsensitive) != 0) {
    box->addItem(name, userData);
  }
}

void SyncOverwriteDialog::readTree(
  const QString &path, DirectoryEntry* dir, QTreeWidgetItem *subTree)
{
  QDir overwrite(path);
  overwrite.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
  QDirIterator dirIter(overwrite);
  while (dirIter.hasNext()) {
    dirIter.next();
    QFileInfo fileInfo = dirIter.fileInfo();

    QString file = fileInfo.fileName();
    if (file == "meta.ini") {
      continue;
    }

    QTreeWidgetItem *newItem = new QTreeWidgetItem(subTree, QStringList(file));

    if (fileInfo.isDir()) {
      DirectoryEntry *subDir = dir->findSubDirectory(ToWString(file));
      if (subDir != nullptr) {
        readTree(fileInfo.absoluteFilePath(), subDir, newItem);
      } else {
        log::error("no directory structure for {}?", file);
        delete newItem;
        newItem = nullptr;
      }
    } else {
      const FileEntryPtr entry = dir->findFile(ToWString(file));
      auto oc = dir->getOriginConnection();

      QComboBox* combo = new QComboBox(ui->syncTree);
      combo->addItem(tr("<don't sync>"), -1);

      if (entry.get() != nullptr) {
        int originID = entry->getOrigin();
        const auto* origin = oc->findByID(originID);
        if (origin) {
          addToComboBox(combo, ToQString(origin->getName()), origin);
        }
        const auto& alternatives = entry->getAlternatives();
        for (auto iter = alternatives.begin(); iter != alternatives.end(); ++iter) {
          const auto* altOrigin = oc->findByID(iter->originID);
          if (altOrigin) {
            addToComboBox(combo, ToQString(altOrigin->getName()), iter->originID);
          }
        }
        combo->setCurrentIndex(combo->count() - 1);
      } else {
        combo->setCurrentIndex(0);
      }
      ui->syncTree->setItemWidget(newItem, 1, combo);
    }
    if (newItem != nullptr) {
      subTree->addChild(newItem);
    }
  }
}


void SyncOverwriteDialog::refresh(const QString &path)
{
  QTreeWidgetItem *rootItem = new QTreeWidgetItem(ui->syncTree, QStringList("<data>"));
  readTree(path, m_DirectoryStructure->root(), rootItem);
  ui->syncTree->addTopLevelItem(rootItem);
  ui->syncTree->expandAll();
}


void SyncOverwriteDialog::applyTo(QTreeWidgetItem *item, const QString &path, const QString &modDirectory)
{
  for (int i = 0; i < item->childCount(); ++i) {
    QTreeWidgetItem *child = item->child(i);
    QString filePath;
    if (path.length() != 0) {
      filePath = path + "/" + child->text(0);
    } else {
      filePath = child->text(0);
    }
    if (child->childCount() != 0) {
      applyTo(child, filePath, modDirectory);
    } else {
      QComboBox *comboBox = qobject_cast<QComboBox*>(ui->syncTree->itemWidget(child, 1));
      if (comboBox != nullptr) {
        int originID = comboBox->itemData(comboBox->currentIndex(), Qt::UserRole).toInt();
        if (originID != -1) {
          const FilesOrigin* origin = m_DirectoryStructure->findOriginByID(originID);
          if (origin) {
            QString source = m_SourcePath + "/" + filePath;
            QString destination = modDirectory + "/" + ToQString(origin->getName()) + "/" + filePath;
            if (!QFile::remove(destination)) {
              reportError(tr("failed to remove %1").arg(destination));
            } else if (!QFile::rename(source, destination)) {
              reportError(tr("failed to move %1 to %2").arg(source).arg(destination));
            }
          }
        }
      }
    }
  }

  QDir dir(m_SourcePath + "/" + path);
  if ((path.length() > 0) && (dir.count() == 2)) {
    dir.rmpath(".");
  }
}


void SyncOverwriteDialog::apply(const QString &modDirectory)
{
  applyTo(ui->syncTree->topLevelItem(0), "", modDirectory);
}
