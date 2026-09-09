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

    if (!BtrfsSnapshots::isOnBtrfs(localPath)) {
        return actions;
    }

    const auto fsRootPathOpt = BtrfsSnapshots::getFsRoot(localPath);
    const auto fsUuidOpt = BtrfsSnapshots::getFsUuid(localPath);

    if (!fsRootPathOpt.has_value() || !fsUuidOpt.has_value()) {
        return actions;
    }

    const QString fsRootPath = fsRootPathOpt.value();
    const QUuid fsUuid = fsUuidOpt.value();

    const auto originalPathOpt = BtrfsSnapshots::getOriginalForFileSnapshot(itemTargetUrl.path(), fsUuid);
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
        if (BtrfsSnapshots::hasSnapshots(itemTargetUrl.toLocalFile(), fsUuid)) {
            auto subvolumeIdOpt = BtrfsSnapshots::getSubvolumeForPath(itemTargetUrl.toLocalFile());
            QAction *action = new QAction(QIcon::fromTheme("view-history"_L1), i18nc("@action:inmenu", "Browse snapshots…"), parentWidget);
            connect(action, &QAction::triggered, this, [this, subvolumeIdOpt, fsRootPath, fsUuid, item]() {
                QUrl targetUrl;
                if (subvolumeIdOpt.has_value() && !fsUuid.isNull()) {
                    targetUrl.setScheme("snapshot"_L1);
                    if (fsRootPath != "/"_L1) {
                        targetUrl.setHost(fsUuid.toString(QUuid::WithoutBraces).toLower());
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
        if (BtrfsSnapshots::hasSnapshots(itemTargetUrl.toLocalFile(), fsUuid)) {
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
