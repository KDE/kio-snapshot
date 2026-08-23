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

QHash<qulonglong, QString> getBtrfsSubvolMounts(const QString &fsRoot)
{
    QHash<qulonglong, QString> subvolMounts;

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

                qulonglong subvol = 5;

                char *val = nullptr;
                size_t sz = 0;
                const char *options = mnt_fs_get_fs_options(fs);

                if (options && mnt_optstr_get_option(options, "subvolid", &val, &sz) == 0 && val) {
                    subvol = QString::fromUtf8(val, sz).toULongLong();
                    if (target) {
                        subvolMounts.insert(subvol, QString::fromUtf8(target));
                    }
                }
            }
        }
    }

    mnt_free_iter(itr);
    mnt_unref_table(tb);

    return subvolMounts;
}

std::optional<qulonglong> BtrfsSnapshots::getSubvolumeForPath(const QString &path, const QString &fsRoot)
{
    enum btrfs_util_error btrfs_err;

    const auto subvolMounts = getBtrfsSubvolMounts(fsRoot);

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsRoot), 0, 0, &iter);
    if (btrfs_err != 0) {
        return std::nullopt;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        const qulonglong subvol = static_cast<qulonglong>(iter_info.id);
        if (subvolMounts.contains(subvol)) {
            if (QDir::cleanPath(path) == QDir::cleanPath(subvolMounts.value(subvol))) {
                return subvol;
            }
        }
        if (QDir::cleanPath(path) == QDir::cleanPath(fsRoot + "/"_L1 + QString::fromUtf8(iter_path))) {
            free(iter_path);
            return subvol;
        }
        free(iter_path);
    }

    if (path == fsRoot) {
        // we did not find a subvolume mounted at fs root, so fs root must be volume 5 (FS_TREE)
        return 5;
    }

    for (const auto &[subvol, subvolPath] : subvolMounts.asKeyValueRange()) {
        if (QDir::cleanPath(path) == QDir::cleanPath(subvolPath)) {
            return subvol;
        }
    }

    if (subvolMounts.contains(5)) {
        const auto &fsTreeMount = subvolMounts.value(5);
        struct btrfs_util_subvolume_iterator *iter;
        btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsTreeMount), 0, 0, &iter);
        if (btrfs_err != 0) {
            return std::nullopt;
        }

        struct btrfs_util_subvolume_info iter_info;
        char *iter_path;
        while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
            if (QDir::cleanPath(path) == QDir::cleanPath(fsTreeMount + "/"_L1 + QString::fromUtf8(iter_path))) {
                free(iter_path);
                return iter_info.id;
            }
            free(iter_path);
        }
    }

    return std::nullopt;
}

std::optional<QString> BtrfsSnapshots::getPathForSubvolume(qulonglong subvolume, const QString &fsRoot)
{
    enum btrfs_util_error btrfs_err;

    const auto subvolMounts = getBtrfsSubvolMounts(fsRoot);
    if (subvolMounts.contains(subvolume)) {
        return subvolMounts.value(subvolume);
    }

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

    if (subvolume == 5 && getSubvolumeForPath(fsRoot, fsRoot) == 5) {
        // 5 is the FS_TREE volume
        return fsRoot;
    }

    if (subvolMounts.contains(5)) {
        const auto &fsTreeMount = subvolMounts.value(5);
        struct btrfs_util_subvolume_iterator *iter;
        btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsTreeMount), 0, 0, &iter);
        if (btrfs_err != 0) {
            return std::nullopt;
        }

        struct btrfs_util_subvolume_info iter_info;
        char *iter_path;
        while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
            if (subvolume == static_cast<qulonglong>(iter_info.id)) {
                QString path = QDir::cleanPath(fsTreeMount + "/"_L1 + QString::fromUtf8(iter_path));
                free(iter_path);
                return path;
            }
            free(iter_path);
        }
    }

    return std::nullopt;
}

bool BtrfsSnapshots::hasSnapshots(const QString &path, const QString &fsRoot)
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
        subvolumeRoot.cdUp();
    }

    if (btrfs_err != 0) {
        return false;
    }

    struct btrfs_util_subvolume_iterator *iter;
    btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsRoot), 0, 0, &iter);
    if (btrfs_err != 0) {
        return false;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        if (QByteArrayView::fromArray(iter_info.parent_uuid) == QByteArrayView::fromArray(subvolume_root_info.uuid)) {
            free(iter_path);
            return true;
        }
        free(iter_path);
    }

    const auto subvolMounts = getBtrfsSubvolMounts(fsRoot);
    if (subvolMounts.contains(5)) {
        const auto &fsTreeMount = subvolMounts.value(5);
        struct btrfs_util_subvolume_iterator *iter;
        btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsTreeMount), 0, 0, &iter);
        if (btrfs_err != 0) {
            return false;
        }

        struct btrfs_util_subvolume_info iter_info;
        char *iter_path;
        while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
            if (QByteArrayView::fromArray(iter_info.parent_uuid) == QByteArrayView::fromArray(subvolume_root_info.uuid)) {
                free(iter_path);
                return true;
            }
            free(iter_path);
        }
    }

    return false;
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

    while ((btrfs_err = btrfs_util_subvolume_get_info(CSTR(subvolumeRoot.absolutePath()), 0, &subvolume_root_info)) != 0) {
        if (subvolumeRoot.isRoot()) {
            break;
        }
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
    QSet<qulonglong> foundSnapshots;
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
                foundSnapshots.insert(static_cast<qulonglong>(iter_info.id));
                fileSnapshots << snapshotInfo;
            }
        }
        free(iter_path);
    }

    const auto subvolMounts = getBtrfsSubvolMounts(fsRoot);
    if (subvolMounts.contains(5)) {
        const auto &fsTreeMount = subvolMounts.value(5);
        struct btrfs_util_subvolume_iterator *iter;
        btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsTreeMount), 0, 0, &iter);
        if (btrfs_err != 0) {
            return fileSnapshots;
        }

        struct btrfs_util_subvolume_info iter_info;
        char *iter_path;
        while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
            if (!foundSnapshots.contains(static_cast<qulonglong>(iter_info.id))
                && QByteArrayView::fromArray(iter_info.parent_uuid) == QByteArrayView::fromArray(subvolume_root_info.uuid)) {
                QString snapshotSubvolPath = QDir::cleanPath(fsTreeMount + "/"_L1 + QString::fromUtf8(iter_path));
                QString filePath = QDir(snapshotSubvolPath).absoluteFilePath(pathRel);
                QFileInfo file(filePath);
                if (file.exists() && file.isReadable()) {
                    FileSnapshot snapshotInfo;
                    snapshotInfo.path = QDir(snapshotSubvolPath).absoluteFilePath(pathRel);
                    snapshotInfo.snapshotted = QDateTime::fromMSecsSinceEpoch(iter_info.otime.tv_sec * 1000 + iter_info.otime.tv_nsec / 1000000);
                    snapshotInfo.modified = file.lastModified();
                    snapshotInfo.subvolumeId = static_cast<qulonglong>(iter_info.id);
                    foundSnapshots.insert(static_cast<qulonglong>(iter_info.id));
                    fileSnapshots << snapshotInfo;
                }
            }
            free(iter_path);
        }
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
    QSet<qulonglong> foundSnapshots;
    while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
        if (QByteArrayView::fromArray(iter_info.parent_uuid) == QByteArrayView::fromArray(info.uuid)) {
            SubvolumeSnapshot snapshotInfo;
            snapshotInfo.path = QDir::cleanPath(fsRoot + "/"_L1 + QString::fromUtf8(iter_path));
            snapshotInfo.subvolumeId = static_cast<qulonglong>(iter_info.id);
            snapshotInfo.snapshotted = QDateTime::fromMSecsSinceEpoch(iter_info.otime.tv_sec * 1000 + iter_info.otime.tv_nsec / 1000000);
            foundSnapshots.insert(static_cast<qulonglong>(iter_info.id));
            subvolSnapshots << snapshotInfo;
        }
        free(iter_path);
    }

    const auto subvolMounts = getBtrfsSubvolMounts(fsRoot);
    if (subvolMounts.contains(5)) {
        const auto &fsTreeMount = subvolMounts.value(5);
        struct btrfs_util_subvolume_iterator *iter;
        btrfs_err = btrfs_util_subvolume_iter_create(CSTR(fsTreeMount), 0, 0, &iter);
        if (btrfs_err != 0) {
            return subvolSnapshots;
        }

        struct btrfs_util_subvolume_info iter_info;
        char *iter_path;
        while ((btrfs_err = btrfs_util_subvolume_iter_next_info(iter, &iter_path, &iter_info)) == 0) {
            if (!foundSnapshots.contains(static_cast<qulonglong>(iter_info.id))
                && QByteArrayView::fromArray(iter_info.parent_uuid) == QByteArrayView::fromArray(info.uuid)) {
                SubvolumeSnapshot snapshotInfo;
                snapshotInfo.path = QDir::cleanPath(fsTreeMount + "/"_L1 + QString::fromUtf8(iter_path));
                snapshotInfo.subvolumeId = static_cast<qulonglong>(iter_info.id);
                snapshotInfo.snapshotted = QDateTime::fromMSecsSinceEpoch(iter_info.otime.tv_sec * 1000 + iter_info.otime.tv_nsec / 1000000);
                subvolSnapshots << snapshotInfo;
            }
            free(iter_path);
        }
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

    struct btrfs_util_subvolume_info root_info;
    btrfs_err = btrfs_util_subvolume_get_info(CSTR(fsRoot), 0, &root_info);
    if (btrfs_err != 0) {
        return subvolumes;
    }
    subvolumes[static_cast<qulonglong>(root_info.id)] = fsRoot;

    return subvolumes;
}
