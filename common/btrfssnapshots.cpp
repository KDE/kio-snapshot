/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "btrfssnapshots.h"

#include <btrfsutil.h>

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QUuid>

using namespace Qt::StringLiterals;

std::optional<qulonglong> BtrfsSnapshots::getSubvolumeForPath(const QString &path)
{
    enum btrfs_util_error btrfs_err;

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create("/", 0, 0, &iter);
    if (btrfs_err != 0) {
        return std::nullopt;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        if (path == "/"_L1 + QString::fromUtf8(iter_path)) {
            free(iter_path);
            return static_cast<qulonglong>(iter_info.id);
        }
        free(iter_path);
    }

    return std::nullopt;
}

std::optional<QString> BtrfsSnapshots::getPathForSubvolume(qulonglong subvolume)
{
    enum btrfs_util_error btrfs_err;

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create("/", 0, 0, &iter);
    if (btrfs_err != 0) {
        return std::nullopt;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        if (subvolume == static_cast<qulonglong>(iter_info.id)) {
            QString path = "/"_L1 + QString::fromUtf8(iter_path);
            free(iter_path);
            return path;
        }
        free(iter_path);
    }

    return std::nullopt;
}

QList<BtrfsSnapshots::FileSnapshot> BtrfsSnapshots::getSnapshotsForFile(const QString &path)
{
    QList<FileSnapshot> fileSnapshots;

    QDir subvolumeRoot(QFileInfo(path).absoluteDir());
    struct btrfs_util_subvolume_info info;
    enum btrfs_util_error btrfs_err;
    while (!subvolumeRoot.isRoot() && (btrfs_err = btrfs_util_subvolume_get_info(subvolumeRoot.absolutePath().toLocal8Bit().constData(), 0, &info)) != 0) {
        subvolumeRoot.cdUp();
    }

    if (btrfs_err != 0) {
        return fileSnapshots;
    }

    QString pathRel = subvolumeRoot.relativeFilePath(path);

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create(subvolumeRoot.absolutePath().toLocal8Bit().constData(), 0, 0, &iter);
    if (btrfs_err != 0) {
        return fileSnapshots;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        if (QByteArrayView::fromArray(iter_info.parent_uuid) == QByteArrayView::fromArray(info.uuid)) {
            QString snapshotSubvolPath = QDir::cleanPath(subvolumeRoot.absolutePath() + "/"_L1 + QString::fromUtf8(iter_path));
            QString filePath = QDir(snapshotSubvolPath).absoluteFilePath(pathRel);
            QFileInfo file(filePath);
            if (file.exists() && file.isFile() && file.isReadable()) {
                FileSnapshot snapshotInfo;
                snapshotInfo.path = QDir(snapshotSubvolPath).absoluteFilePath(pathRel);
                snapshotInfo.snapshotted = QDateTime::fromMSecsSinceEpoch(iter_info.otime.tv_sec * 1000 + iter_info.otime.tv_nsec / 1000000);
                snapshotInfo.modified = file.lastModified();
                snapshotInfo.subvolumeId = static_cast<qulonglong>(iter_info.id);
                fileSnapshots << snapshotInfo;
            }
        }
        free(iter_path);
    }

    return fileSnapshots;
}

QList<BtrfsSnapshots::SubvolumeSnapshot> BtrfsSnapshots::getSnapshotsForSubvolume(const QString &path)
{
    QList<SubvolumeSnapshot> subvolSnapshots;

    struct btrfs_util_subvolume_info info;
    enum btrfs_util_error btrfs_err;

    btrfs_err = btrfs_util_subvolume_get_info(path.toLocal8Bit().constData(), 0, &info);
    if (btrfs_err != 0) {
        return subvolSnapshots;
    }

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create(path.toLocal8Bit().constData(), 0, 0, &iter);
    if (btrfs_err != 0) {
        return subvolSnapshots;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        if (QByteArrayView::fromArray(iter_info.parent_uuid) == QByteArrayView::fromArray(info.uuid)) {
            SubvolumeSnapshot snapshotInfo;
            snapshotInfo.path = QString::fromUtf8(iter_path);
            snapshotInfo.subvolumeId = static_cast<qulonglong>(iter_info.id);
            snapshotInfo.snapshotted = QDateTime::fromMSecsSinceEpoch(iter_info.otime.tv_sec * 1000 + iter_info.otime.tv_nsec / 1000000);
            subvolSnapshots << snapshotInfo;
        }
        free(iter_path);
    }

    return subvolSnapshots;
}

QMap<qulonglong, QString> BtrfsSnapshots::getNonSnapshotSubvolumes()
{
    QMap<qulonglong, QString> subvolumes;

    enum btrfs_util_error btrfs_err;

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create("/", 0, 0, &iter);
    if (btrfs_err != 0) {
        return subvolumes;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        if (QUuid::fromBytes(iter_info.parent_uuid).isNull()) {
            subvolumes[static_cast<qulonglong>(iter_info.id)] = "/"_L1 + QString::fromUtf8(iter_path);
        }
        free(iter_path);
    }

    return subvolumes;
}
