/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "snapshotfileitemaction.h"
#include "debug.h"
#include "filesnapshotdialog.h"

#include "../common/snapshotinfotypes.h"

#include "service_interface.h"

#include <KIO/JobUiDelegate>
#include <KIO/OpenUrlJob>

#include <KFileItem>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>
#include <QIcon>
#include <QList>
#include <QMenu>
#include <QUrl>

using namespace Qt::StringLiterals;

K_PLUGIN_CLASS_WITH_JSON(SnapshotFileItemAction, "snapshotfileitemaction.json")

SnapshotFileItemAction::SnapshotFileItemAction(QObject *parent)
    : KAbstractFileItemActionPlugin(parent)
    , service(new org::kde::ksnapshotservice("org.kde.ksnapshotservice"_L1, "/KSnapshotService"_L1, QDBusConnection::systemBus(), this))
{
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
            return actions;
        }
        qulonglong subvolumeId = subvolumeIdReply.value();
        auto subvolumePathReply = service->getPathForSubvolume(subvolumeId);
        subvolumePathReply.waitForFinished();
        if (!subvolumePathReply.isValid()) {
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
            return actions;
        }
        QVariantList snapshotsVars = snapshotQueryReply.value();
        QList<FileSnapshotInfo> snapshotInfos;
        for (const QVariant &snapshotV : snapshotsVars) {
            QVariantMap snapshotMap = qdbus_cast<QVariantMap>(snapshotV.value<QDBusArgument>());
            FileSnapshotInfo snapshotInfo;
            snapshotInfo.path = snapshotMap.value("Path"_L1).toString();
            snapshotInfo.generation = snapshotMap.value("Generation"_L1).toULongLong();
            if (snapshotMap.contains("SnapshotCreationTimeSecs"_L1)) {
                snapshotInfo.snapshotTimeSecs = snapshotMap.value("SnapshotCreationTimeSecs"_L1).toULongLong();
                snapshotInfo.snapshotTimeNanosecs = snapshotMap.value("SnapshotCreationTimeNanosecs"_L1).toULongLong();
            } else {
                snapshotInfo.snapshotTimeSecs = std::nullopt;
                snapshotInfo.snapshotTimeNanosecs = std::nullopt;
            }
            snapshotInfo.modificationTimeSecs = snapshotMap.value("ModificationTimeSecs"_L1).toULongLong();
            snapshotInfo.modificationTimeNanosecs = snapshotMap.value("ModificationTimeNanosecs"_L1).toULongLong();
            snapshotInfos << snapshotInfo;
        }

        QAction *action = new QAction(QIcon::fromTheme("view-history"_L1), i18n("View snapshots…"), parentWidget);
        connect(action, &QAction::triggered, this, [parentWidget, itemUrl, snapshotInfos]() {
            FileSnapshotDialog *dlg = new FileSnapshotDialog(itemUrl, snapshotInfos, parentWidget);
            dlg->show();
            dlg->raise();
            dlg->activateWindow();
        });

        actions << action;
    }

    return actions;
}

#include "snapshotfileitemaction.moc"
