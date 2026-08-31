/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "btrfssnapshots.h"

#include <stdlib.h>
#include <string.h>

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

#include <libmount/libmount.h>

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QString>
#include <QUuid>

#include <Solid/Block>
#include <Solid/Device>
#include <Solid/StorageAccess>
#include <Solid/StorageVolume>

using namespace Qt::StringLiterals;

#define CSTR(s) (s.toLocal8Bit().constData())

std::optional<QString> getDeviceForRoot(const QString &fsRoot)
{
    auto fsDevice = Solid::Device::storageAccessFromPath(fsRoot);

    auto fsBlock = fsDevice.as<Solid::Block>();
    if (!fsBlock) {
        return std::nullopt;
    }

    return fsBlock->device();
}

std::optional<QPair<QDir, struct btrfs_util_subvolume_info>> getSubvolumeRoot(const QString &path)
{
    QDir subvolumeRoot;
    QFileInfo fileInfo(path);
    if (fileInfo.isDir()) {
        subvolumeRoot = QDir(path);
    } else {
        subvolumeRoot = QDir(QFileInfo(path).absoluteDir());
    }

    struct btrfs_util_subvolume_info subvolume_root_info;
    enum btrfs_util_error btrfs_err;

    while ((btrfs_err = btrfs_util_subvolume_get_info(CSTR(subvolumeRoot.absolutePath()), 0, &subvolume_root_info)) != 0) {
        if (subvolumeRoot.isRoot()) {
            break;
        }
        bool ok = subvolumeRoot.cdUp();
        if (!ok) {
            break;
        }
    }

    if (btrfs_err != 0) {
        return std::nullopt;
    }

    return std::make_pair(subvolumeRoot, subvolume_root_info);
}

QList<QString> getBtrfsSubvolMounts(const QString &fsRoot)
{
    QList<QString> subvolMounts;
    subvolMounts << fsRoot;

    auto deviceOpt = getDeviceForRoot(fsRoot);
    if (!deviceOpt.has_value()) {
        return subvolMounts;
    }

    const QString &devicePath = deviceOpt.value();

    struct libmnt_table *tb = mnt_new_table();
    struct libmnt_iter *itr = mnt_new_iter(MNT_ITER_FORWARD);
    struct libmnt_fs *fs;

    if (!tb || !itr) {
        if (tb)
            mnt_unref_table(tb);
        if (itr)
            mnt_free_iter(itr);
        return subvolMounts;
    }

    if (mnt_table_parse_file(tb, "/proc/self/mountinfo") == 0) {
        while (mnt_table_next_fs(tb, itr, &fs) == 0) {
            const char *fstype = mnt_fs_get_fstype(fs);

            if (!fstype || strcmp(fstype, "btrfs") != 0) {
                continue;
            }

            const char *src = mnt_fs_get_source(fs);
            if (!src) {
                continue;
            }

            char *resolvedSrc = realpath(src, nullptr);
            QString srcStr;
            if (resolvedSrc) {
                srcStr = QString::fromUtf8(resolvedSrc);
                free(resolvedSrc);
            } else {
                srcStr = QString::fromUtf8(src);
            }

            if (srcStr == devicePath) {
                const char *target = mnt_fs_get_target(fs);
                const QString targetStr = QString::fromUtf8(target);
                if (targetStr != fsRoot) {
                    subvolMounts << targetStr;
                }
            }
        }
    }

    mnt_free_iter(itr);
    mnt_unref_table(tb);

    std::sort(subvolMounts.begin(), subvolMounts.end(), [](const QString &a, const QString &b) {
        return a.length() < b.length();
    });
    return subvolMounts;
}

std::optional<qulonglong> BtrfsSnapshots::getSubvolumeForPath(const QString &path, const QString &fsRoot)
{
    enum btrfs_util_error btrfs_err;

    struct btrfs_util_subvolume_info info;
    btrfs_err = btrfs_util_subvolume_get_info(CSTR(path), 0, &info);
    if (btrfs_err == 0) {
        return info.id;
    } else if (path == fsRoot) {
        return 5;
    } else {
        return std::nullopt;
    }
}

std::optional<QString> BtrfsSnapshots::getPathForSubvolume(qulonglong subvolume, const QString &fsRoot)
{
    enum btrfs_util_error btrfs_err;

    const auto subvolMounts = getBtrfsSubvolMounts(fsRoot);

    for (const auto &mountPoint : subvolMounts) {
        struct btrfs_util_subvolume_info info;
        btrfs_err = btrfs_util_subvolume_get_info(CSTR(mountPoint), 0, &info);
        if (btrfs_err == 0 && info.id == subvolume) {
            return mountPoint;
        }

        struct btrfs_util_subvolume_iterator *iter;
        btrfs_err = btrfs_util_subvolume_iter_create(CSTR(mountPoint), 0, 0, &iter);
        if (btrfs_err != 0) {
            continue;
        }

        struct btrfs_util_subvolume_info iter_info;
        char *iter_path;
        while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
            if (subvolume == static_cast<qulonglong>(iter_info.id)) {
                QString path = QDir::cleanPath(mountPoint + "/"_L1 + QString::fromUtf8(iter_path));
                free(iter_path);
                return path;
            }
            free(iter_path);
        }
    }

    if (getSubvolumeForPath(fsRoot, fsRoot) == subvolume) {
        return fsRoot;
    }

    return std::nullopt;
}

bool BtrfsSnapshots::hasSnapshots(const QString &path, const QString &fsRoot)
{
    enum btrfs_util_error btrfs_err;

    const auto subvolume_root_info_opt = getSubvolumeRoot(path);
    if (!subvolume_root_info_opt.has_value()) {
        return false;
    }

    const auto subvolMounts = getBtrfsSubvolMounts(fsRoot);
    const auto [subvolumeRoot, subvolume_root_info] = subvolume_root_info_opt.value();

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsRoot), 0, 0, &iter);
    if (btrfs_err != 0) {
        return false;
    }

    for (const auto &mountPoint : subvolMounts) {
        struct btrfs_util_subvolume_info info;
        btrfs_err = btrfs_util_subvolume_get_info(CSTR(mountPoint), 0, &info);
        if (btrfs_err == 0 && QByteArrayView::fromArray(info.parent_uuid) == QByteArrayView::fromArray(subvolume_root_info.uuid)) {
            return true;
        }

        struct btrfs_util_subvolume_iterator *iter;
        btrfs_err = btrfs_util_subvolume_iter_create(CSTR(mountPoint), 0, 0, &iter);
        if (btrfs_err != 0) {
            continue;
        }

        struct btrfs_util_subvolume_info iter_info;
        char *iter_path;
        while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
            free(iter_path);
            if (QByteArrayView::fromArray(iter_info.parent_uuid) == QByteArrayView::fromArray(subvolume_root_info.uuid)) {
                return true;
            }
        }
    }

    return false;
}

QList<BtrfsSnapshots::FileSnapshot> BtrfsSnapshots::getSnapshotsForFile(const QString &path, const QString &fsRoot)
{
    QList<FileSnapshot> fileSnapshots;
    enum btrfs_util_error btrfs_err;

    const auto subvolume_root_info_opt = getSubvolumeRoot(path);
    if (!subvolume_root_info_opt.has_value()) {
        return fileSnapshots;
    }

    const auto [subvolumeRoot, subvolume_root_info] = subvolume_root_info_opt.value();

    QString pathRel = subvolumeRoot.relativeFilePath(path);

    const auto subvolMounts = getBtrfsSubvolMounts(fsRoot);
    QMap<qulonglong, QPair<QString, struct btrfs_util_subvolume_info>> foundSnapshots;
    for (const auto &mountPoint : subvolMounts) {
        struct btrfs_util_subvolume_info info;
        btrfs_err = btrfs_util_subvolume_get_info(CSTR(mountPoint), 0, &info);
        if (btrfs_err == 0 && QByteArrayView::fromArray(info.parent_uuid) == QByteArrayView::fromArray(subvolume_root_info.uuid)) {
            foundSnapshots.insert(info.id, {mountPoint, info});
        }

        struct btrfs_util_subvolume_iterator *iter;
        btrfs_err = btrfs_util_subvolume_iter_create(CSTR(mountPoint), 0, 0, &iter);
        if (btrfs_err != 0) {
            continue;
        }

        struct btrfs_util_subvolume_info iter_info;
        char *iter_path;
        while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
            if (QByteArrayView::fromArray(iter_info.parent_uuid) == QByteArrayView::fromArray(subvolume_root_info.uuid)
                && !foundSnapshots.contains(iter_info.id)) {
                foundSnapshots.insert(iter_info.id, {QDir::cleanPath(mountPoint + "/"_L1 + QString::fromUtf8(iter_path)), iter_info});
            }
            free(iter_path);
        }
    }

    for (const auto &[path, info] : std::as_const(foundSnapshots)) {
        QString filePath = QDir(path).absoluteFilePath(pathRel);
        QFileInfo file(filePath);
        if (file.exists() && file.isReadable()) {
            FileSnapshot snapshotInfo;
            snapshotInfo.path = filePath;
            snapshotInfo.snapshotted = QDateTime::fromMSecsSinceEpoch(info.otime.tv_sec * 1000 + info.otime.tv_nsec / 1000000);
            snapshotInfo.modified = file.lastModified();
            snapshotInfo.subvolumeId = info.id;
            fileSnapshots << snapshotInfo;
        }
    }

    return fileSnapshots;
}

std::optional<QString> BtrfsSnapshots::getOriginalForFileSnapshot(const QString &fileSnapshotPath, const QString &fsRoot)
{
    enum btrfs_util_error btrfs_err;

    const auto subvolume_root_info_opt = getSubvolumeRoot(fileSnapshotPath);
    if (!subvolume_root_info_opt.has_value()) {
        return std::nullopt;
    }

    const auto [subvolumeRoot, subvolume_root_info] = subvolume_root_info_opt.value();

    QUuid snapshotOfUuid = QUuid::fromBytes(subvolume_root_info.parent_uuid);
    if (snapshotOfUuid.isNull()) {
        return std::nullopt;
    }

    QString pathRel = subvolumeRoot.relativeFilePath(fileSnapshotPath);

    const auto subvolMounts = getBtrfsSubvolMounts(fsRoot);

    for (const auto &mountPoint : subvolMounts) {
        struct btrfs_util_subvolume_info info;
        btrfs_err = btrfs_util_subvolume_get_info(CSTR(mountPoint), 0, &info);
        if (btrfs_err == 0 && QUuid::fromBytes(info.uuid) == snapshotOfUuid) {
            return QDir(mountPoint).absoluteFilePath(pathRel);
        }

        struct btrfs_util_subvolume_iterator *iter;
        btrfs_err = btrfs_util_subvolume_iter_create(CSTR(mountPoint), 0, 0, &iter);
        if (btrfs_err != 0) {
            continue;
        }

        struct btrfs_util_subvolume_info iter_info;
        char *iter_path;
        while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
            if (QUuid::fromBytes(iter_info.uuid) == snapshotOfUuid) {
                const QString subvolumePath = QDir::cleanPath(mountPoint + "/"_L1 + QString::fromUtf8(iter_path));
                free(iter_path);
                return QDir(subvolumePath).absoluteFilePath(pathRel);
            }
            free(iter_path);
        }
    }

    return std::nullopt;
}

QList<BtrfsSnapshots::SubvolumeSnapshot> BtrfsSnapshots::getSnapshotsForSubvolume(const QString &path, const QString &fsRoot)
{
    QList<SubvolumeSnapshot> subvolSnapshots;

    struct btrfs_util_subvolume_info subvolume_root_info;
    enum btrfs_util_error btrfs_err;

    btrfs_err = btrfs_util_subvolume_get_info(CSTR(path), 0, &subvolume_root_info);
    if (btrfs_err != 0) {
        return subvolSnapshots;
    }

    const auto subvolMounts = getBtrfsSubvolMounts(fsRoot);
    QMap<qulonglong, QPair<QString, struct btrfs_util_subvolume_info>> foundSnapshots;
    for (const auto &mountPoint : subvolMounts) {
        struct btrfs_util_subvolume_info info;
        btrfs_err = btrfs_util_subvolume_get_info(CSTR(mountPoint), 0, &info);
        if (btrfs_err == 0 && QByteArrayView::fromArray(info.parent_uuid) == QByteArrayView::fromArray(subvolume_root_info.uuid)) {
            foundSnapshots.insert(info.id, {mountPoint, info});
        }

        struct btrfs_util_subvolume_iterator *iter;
        btrfs_err = btrfs_util_subvolume_iter_create(CSTR(mountPoint), 0, 0, &iter);
        if (btrfs_err != 0) {
            continue;
        }

        struct btrfs_util_subvolume_info iter_info;
        char *iter_path;
        while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
            if (QByteArrayView::fromArray(iter_info.parent_uuid) == QByteArrayView::fromArray(subvolume_root_info.uuid)
                && !foundSnapshots.contains(iter_info.id)) {
                foundSnapshots.insert(iter_info.id, {QDir::cleanPath(mountPoint + "/"_L1 + QString::fromUtf8(iter_path)), iter_info});
            }
            free(iter_path);
        }
    }

    for (const auto &[path, info] : std::as_const(foundSnapshots)) {
        SubvolumeSnapshot snapshotInfo;
        snapshotInfo.path = path;
        snapshotInfo.subvolumeId = info.id;
        snapshotInfo.snapshotted = QDateTime::fromMSecsSinceEpoch(info.otime.tv_sec * 1000 + info.otime.tv_nsec / 1000000);
        subvolSnapshots << snapshotInfo;
    }

    return subvolSnapshots;
}

QMap<qulonglong, QString> BtrfsSnapshots::getNonSnapshotSubvolumes(const QString &fsRoot)
{
    QMap<qulonglong, QString> subvolumes;

    enum btrfs_util_error btrfs_err;

    const auto subvolMounts = getBtrfsSubvolMounts(fsRoot);
    for (const auto &mountPoint : subvolMounts) {
        struct btrfs_util_subvolume_info info;
        btrfs_err = btrfs_util_subvolume_get_info(CSTR(mountPoint), 0, &info);
        if (btrfs_err == 0) {
            subvolumes[info.id] = mountPoint;
        }

        struct btrfs_util_subvolume_iterator *iter;
        btrfs_err = btrfs_util_subvolume_iter_create(CSTR(mountPoint), 0, 0, &iter);
        if (btrfs_err != 0) {
            continue;
        }

        struct btrfs_util_subvolume_info iter_info;
        char *iter_path;
        while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
            if (QUuid::fromBytes(iter_info.parent_uuid).isNull() && !subvolumes.contains(iter_info.id)) {
                subvolumes[iter_info.id] = QDir::cleanPath(mountPoint + "/"_L1 + QString::fromUtf8(iter_path));
            }
            free(iter_path);
        }
    }

    return subvolumes;
}
