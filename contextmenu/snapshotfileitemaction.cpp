/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "snapshotfileitemaction.h"
#include "debug.h"

#include "../common/snapshotinfotypes.h"

#include "service_interface.h"

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
    qDBusRegisterMetaType<FileSnapshotInfo>();
    qDBusRegisterMetaType<QList<FileSnapshotInfo>>();
    qDBusRegisterMetaType<SubvolumeSnapshotInfo>();
    qDBusRegisterMetaType<QList<SubvolumeSnapshotInfo>>();
    service = new org::kde::ksnapshotservice("org.kde.ksnapshotservice"_L1, "/KSnapshotService"_L1, QDBusConnection::systemBus(), this);
}

QList<QAction *> SnapshotFileItemAction::actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget)
{
    QList<QAction *> actions;

    if (fileItemInfos.urlList().length() != 1) {
        return actions;
    }

    QUrl itemUrl = fileItemInfos.urlList().first();
    KFileItem item = fileItemInfos.items().findByUrl(itemUrl);
    if (itemUrl.scheme() == "snapshot"_L1) {
        return actions;
    }

    if (item.isDir()) {
        auto subvolumeIdReply = service->getSubvolumeForPath(itemUrl.toLocalFile());
        subvolumeIdReply.waitForFinished();
        if (!subvolumeIdReply.isValid()) {
            qCDebug(SNAPSHOT_FILEITEMACTION()) << "invalid reply for getSubvolumeForPath" << itemUrl;
            return actions;
        }
        qulonglong subvolumeId = subvolumeIdReply.value();
        auto subvolumePathReply = service->getPathForSubvolume(subvolumeId);
        subvolumePathReply.waitForFinished();
        if (!subvolumePathReply.isValid()) {
            qCDebug(SNAPSHOT_FILEITEMACTION()) << "invalid reply for getPathForSubvolume" << subvolumeId;
            return actions;
        }
        qCDebug(SNAPSHOT_FILEITEMACTION()) << subvolumePathReply.value() << itemUrl << itemUrl.toLocalFile();
        if (subvolumePathReply.value() == itemUrl.toLocalFile()) {
            // we are at the root of a subvolume
            auto snapshotReply = service->getSnapshotsForSubvolume(subvolumeId);
            snapshotReply.waitForFinished();
            if (snapshotReply.isValid() && !snapshotReply.value().isEmpty()) {
                QAction *action = new QAction(QIcon::fromTheme("view-history"_L1), i18n("Browse snapshots…"), parentWidget);
                connect(action, &QAction::triggered, this, [this, subvolumeId]() {
                    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(QUrl("snapshot://subvolume%1"_L1.arg(QString::number(subvolumeId))), "inode/directory"_L1, this);
                    job->start();
                });
                actions << action;
            }
        }
    } else if (item.isLocalFile()) {
        auto snapshotQueryReply = service->getSnapshotsForFile(itemUrl.toLocalFile());
        snapshotQueryReply.waitForFinished();
        if (!snapshotQueryReply.isValid()) {
            qCDebug(SNAPSHOT_FILEITEMACTION()) << "invalid reply for getSnapshotsForFile" << itemUrl;
            return actions;
        }
        // QList<FileSnapshotInfo> snapshotInfos = snapshotQueryReply.value();

        QAction *action = new QAction(QIcon::fromTheme("view-history"_L1), i18n("View snapshots…"), parentWidget);
        connect(action, &QAction::triggered, this, [this, item]() {
            KIO::OpenUrlJob *job = new KIO::OpenUrlJob(QUrl("filesnapshots://%1"_L1.arg(item.localPath())), "inode/directory"_L1, this);
            job->start();
        });

        actions << action;
    }

    return actions;
}

#include "snapshotfileitemaction.moc"
