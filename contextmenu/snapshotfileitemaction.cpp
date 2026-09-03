/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "snapshotfileitemaction.h"
#include "debug.h"

#include <btrfssnapshots.h>

#include <KIO/CopyJob>
#include <KIO/JobUiDelegate>
#include <KIO/OpenUrlJob>

#include <Solid/Device>
#include <Solid/StorageAccess>
#include <Solid/StorageVolume>

#include <KFileItem>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>
#include <QDBusMetaType>
#include <QDir>
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

    const KFileItem item = fileItemInfos.items().constFirst();

    // for a snapshot:// URL, this will point to the actual snapshot location, not the virtual snapshot:// URL
    const QUrl itemTargetUrl = item.mostLocalUrl();

    // and this will be the virtual snapshot:// URL (with the specific snapshot's internal name appended in case of snapshot:///file/...)
    const QUrl itemUrl = item.url();

    QString localPath = itemTargetUrl.toLocalFile();
    auto fsDevice = Solid::Device::storageAccessFromPath(localPath);
    auto fsAccess = fsDevice.as<Solid::StorageAccess>();
    if (!fsAccess) {
        qCCritical(SNAPSHOT_FILEITEMACTION()) << "could not determine fs root path for" << localPath;
        return actions;
    }
    QString fsRootPath = fsAccess->filePath();
    auto fsVolume = fsDevice.as<Solid::StorageVolume>();
    if (!fsVolume) {
        qCCritical(SNAPSHOT_FILEITEMACTION()) << "could not determine fs storage volume for" << localPath;
        return actions;
    }
    if (fsVolume->fsType() != "btrfs"_L1) {
        return actions;
    }
    QString fsUuid = fsVolume->uuid();

    const auto originalPathOpt = BtrfsSnapshots::getOriginalForFileSnapshot(itemTargetUrl.path(), fsRootPath);
    if (originalPathOpt.has_value()) {
        const auto originalPath = originalPathOpt.value();
        QAction *action = new QAction(QIcon::fromTheme("document-revert"_L1), i18nc("@action:inmenu", "Restore…"), parentWidget);
        connect(action, &QAction::triggered, this, [originalPath, itemTargetUrl]() {
            auto *job = KIO::copy(itemTargetUrl, QUrl::fromLocalFile(originalPath));
            job->start();
        });
        actions << action;
    }

    if (item.isDir()) {
        if (BtrfsSnapshots::hasSnapshots(itemTargetUrl.toLocalFile(), fsRootPath)) {
            auto subvolumeIdOpt = BtrfsSnapshots::getSubvolumeForPath(itemTargetUrl.toLocalFile(), fsRootPath);
            QAction *action = new QAction(QIcon::fromTheme("view-history"_L1), i18nc("@action:inmenu", "Browse snapshots…"), parentWidget);
            connect(action, &QAction::triggered, this, [this, subvolumeIdOpt, fsRootPath, fsUuid, item]() {
                QUrl targetUrl;
                if (subvolumeIdOpt.has_value()) {
                    targetUrl.setScheme("snapshot"_L1);
                    if (fsRootPath != "/"_L1) {
                        targetUrl.setHost(fsUuid);
                    }
                    targetUrl.setPath("/subvolume/"_L1 + QString::number(subvolumeIdOpt.value()));
                } else {
                    targetUrl.setScheme("snapshot"_L1);
                    targetUrl.setPath(QDir::cleanPath("/file/"_L1 + item.localPath()));
                }
                KIO::OpenUrlJob *job = new KIO::OpenUrlJob(targetUrl, "inode/directory"_L1, this);
                job->start();
            });
            actions << action;
        }
    } else if (item.isLocalFile()) {
        if (BtrfsSnapshots::hasSnapshots(itemTargetUrl.toLocalFile(), fsRootPath)) {
            QAction *action = new QAction(QIcon::fromTheme("view-history"_L1), i18nc("@action:inmenu", "View snapshots…"), parentWidget);
            connect(action, &QAction::triggered, this, [this, item]() {
                QUrl targetUrl;
                targetUrl.setScheme("snapshot"_L1);
                targetUrl.setPath(QDir::cleanPath("/file/"_L1 + item.localPath()));
                KIO::OpenUrlJob *job = new KIO::OpenUrlJob(targetUrl, "inode/directory"_L1, this);
                job->start();
            });
            actions << action;
        }
    }

    return actions;
}

#include "snapshotfileitemaction.moc"
