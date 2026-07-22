/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "btrfssnapshots.h"

#include <btrfsutil.h>

#define BTRFS_UTIL_VERSION ( \
(BTRFS_UTIL_VERSION_MAJOR * 10000) + \
(BTRFS_UTIL_VERSION_MINOR * 100) + \
(BTRFS_UTIL_VERSION_PATCH))

#if BTRFS_UTIL_VERSION < 10302
    #define btrfs_util_subvolume_get_info btrfs_util_subvolume_info
    #define btrfs_util_subvolume_iter_next_info btrfs_util_subvolume_iterator_next_info
    #define btrfs_util_subvolume_iter_create btrfs_util_create_subvolume_iterator
#endif

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QUuid>

using namespace Qt::StringLiterals;

#define CSTR(s) (s.toLocal8Bit().constData())

std::optional<qulonglong> BtrfsSnapshots::getSubvolumeForPath(const QString &path, const QString &fsRoot)
{
    enum btrfs_util_error btrfs_err;

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsRoot), 0, 0, &iter);
    if (btrfs_err != 0) {
        return std::nullopt;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        if (path == QDir::cleanPath(fsRoot + "/"_L1 + QString::fromUtf8(iter_path))) {
            free(iter_path);
            return static_cast<qulonglong>(iter_info.id);
        }
        free(iter_path);
    }

    return std::nullopt;
}

std::optional<QString> BtrfsSnapshots::getPathForSubvolume(qulonglong subvolume, const QString &fsRoot)
{
    enum btrfs_util_error btrfs_err;

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsRoot), 0, 0, &iter);
    if (btrfs_err != 0) {
        return std::nullopt;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        if (subvolume == static_cast<qulonglong>(iter_info.id)) {
            QString path = QDir::cleanPath(fsRoot + "/"_L1 + QString::fromUtf8(iter_path));
            free(iter_path);
            return path;
        }
        free(iter_path);
    }

    return std::nullopt;
}

QList<BtrfsSnapshots::FileSnapshot> BtrfsSnapshots::getSnapshotsForFile(const QString &path, const QString &fsRoot)
{
    QList<FileSnapshot> fileSnapshots;
    QDir subvolumeRoot;
    QFileInfo fileInfo(path);
    if (fileInfo.isDir()) {
        subvolumeRoot = QDir(path);
    } else {
        subvolumeRoot = QDir(QFileInfo(path).absoluteDir());
    }
    struct btrfs_util_subvolume_info subvolume_root_info;
    enum btrfs_util_error btrfs_err;
    while (!subvolumeRoot.isRoot() && (btrfs_err = btrfs_util_subvolume_get_info(CSTR(subvolumeRoot.absolutePath()), 0, &subvolume_root_info)) != 0) {
        subvolumeRoot.cdUp();
    }

    if (btrfs_err != 0) {
        return fileSnapshots;
    }

    QString pathRel = subvolumeRoot.relativeFilePath(path);

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsRoot), 0, 0, &iter);
    if (btrfs_err != 0) {
        return fileSnapshots;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        if (QByteArrayView::fromArray(iter_info.parent_uuid) == QByteArrayView::fromArray(subvolume_root_info.uuid)) {
            QString snapshotSubvolPath = QDir::cleanPath(fsRoot + "/"_L1 + QString::fromUtf8(iter_path));
            QString filePath = QDir(snapshotSubvolPath).absoluteFilePath(pathRel);
            QFileInfo file(filePath);
            if (file.exists() && file.isReadable()) {
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

QList<BtrfsSnapshots::SubvolumeSnapshot> BtrfsSnapshots::getSnapshotsForSubvolume(const QString &path, const QString &fsRoot)
{
    QList<SubvolumeSnapshot> subvolSnapshots;

    struct btrfs_util_subvolume_info info;
    enum btrfs_util_error btrfs_err;

    btrfs_err = btrfs_util_subvolume_get_info(CSTR(path), 0, &info);
    if (btrfs_err != 0) {
        return subvolSnapshots;
    }

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsRoot), 0, 0, &iter);
    if (btrfs_err != 0) {
        return subvolSnapshots;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        if (QByteArrayView::fromArray(iter_info.parent_uuid) == QByteArrayView::fromArray(info.uuid)) {
            SubvolumeSnapshot snapshotInfo;
            snapshotInfo.path = QDir::cleanPath(fsRoot + "/"_L1 + QString::fromUtf8(iter_path));
            snapshotInfo.subvolumeId = static_cast<qulonglong>(iter_info.id);
            snapshotInfo.snapshotted = QDateTime::fromMSecsSinceEpoch(iter_info.otime.tv_sec * 1000 + iter_info.otime.tv_nsec / 1000000);
            subvolSnapshots << snapshotInfo;
        }
        free(iter_path);
    }

    return subvolSnapshots;
}

QMap<qulonglong, QString> BtrfsSnapshots::getNonSnapshotSubvolumes(const QString &fsRoot)
{
    QMap<qulonglong, QString> subvolumes;

    enum btrfs_util_error btrfs_err;

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsRoot), 0, 0, &iter);
    if (btrfs_err != 0) {
        return subvolumes;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        if (QUuid::fromBytes(iter_info.parent_uuid).isNull()) {
            subvolumes[static_cast<qulonglong>(iter_info.id)] = QDir::cleanPath(fsRoot + "/"_L1 + QString::fromUtf8(iter_path));
        }
        free(iter_path);
    }

    return subvolumes;
}
