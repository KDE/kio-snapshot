/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "snapshotfileitemaction.h"
#include "debug.h"

#include <btrfssnapshots.h>

#include <KIO/JobUiDelegate>
#include <KIO/OpenUrlJob>

#include <KFileItem>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>
#include <QDBusMetaType>
#include <QIcon>
#include <QList>
#include <QMenu>
#include <QUrl>

using namespace Qt::StringLiterals;

K_PLUGIN_CLASS_WITH_JSON(SnapshotFileItemAction, "snapshotfileitemaction.json")

SnapshotFileItemAction::SnapshotFileItemAction(QObject *parent)
    : KAbstractFileItemActionPlugin(parent)
{
}

QList<QAction *> SnapshotFileItemAction::actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget)
{
    QList<QAction *> actions;

    if (fileItemInfos.urlList().length() != 1) {
        return actions;
    }

    QUrl itemUrl = fileItemInfos.urlList().constFirst();
    KFileItem item = fileItemInfos.items().findByUrl(itemUrl);
    if (itemUrl.scheme() == "snapshot"_L1) {
        return actions;
    }

    if (item.isDir()) {
        if (!BtrfsSnapshots::getSnapshotsForSubvolume(itemUrl.toLocalFile()).empty()) {
            auto subvolumeIdOpt = BtrfsSnapshots::getSubvolumeForPath(itemUrl.toLocalFile());
            if (!subvolumeIdOpt.has_value()) {
                qCCritical(SNAPSHOT_FILEITEMACTION()) << "found snapshots for dir" << itemUrl.toLocalFile() << "but it did not have a subvolume id";
                return actions;
            }
            auto subvolumeId = subvolumeIdOpt.value();
            QAction *action = new QAction(QIcon::fromTheme("view-history"_L1), i18nc("@action:inmenu", "Browse snapshots…"), parentWidget);
            connect(action, &QAction::triggered, this, [this, subvolumeId]() {
                KIO::OpenUrlJob *job = new KIO::OpenUrlJob(QUrl("snapshot:///%1"_L1.arg(QString::number(subvolumeId))), "inode/directory"_L1, this);
                job->start();
            });
            actions << action;
        }
    } else if (item.isLocalFile()) {
        if (!BtrfsSnapshots::getSnapshotsForFile(itemUrl.toLocalFile()).empty()) {
            QAction *action = new QAction(QIcon::fromTheme("view-history"_L1), i18nc("@action:inmenu", "View snapshots…"), parentWidget);
            connect(action, &QAction::triggered, this, [this, item]() {
                KIO::OpenUrlJob *job = new KIO::OpenUrlJob(QUrl("filesnapshots://%1"_L1.arg(item.localPath())), "inode/directory"_L1, this);
                job->start();
            });
            actions << action;
        }
    }

    return actions;
}

#include "snapshotfileitemaction.moc"
