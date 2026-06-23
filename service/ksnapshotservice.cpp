/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "ksnapshotservice.h"
#include "ksnapshotservice_debug.h"

#include "../common/snapshotinfotypes.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDir>
#include <QFileInfo>
#include <QString>

#include <cstring>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <btrfsutil.h>

using namespace Qt::StringLiterals;

int openDirForUser(const QString &path, uint userId)
{
    // we use a file descriptor here to avoid time-of-check-to-time-of-use problems
    // since various functions in libbtrfsutil can take the same directory fd that we check
    int fd = open(path.toUtf8().constData(), O_RDONLY | O_DIRECTORY);
    struct stat sb;
    if (fd == -1 || fstat(fd, &sb) == -1 || sb.st_uid != userId) {
        if (fd != -1) {
            close(fd);
        }
        return -1;
    }

    return fd;
}

std::optional<QString> getRootSubvolumePath()
{
    enum btrfs_util_error btrfs_err;

    char *root_path;
    btrfs_err = btrfs_util_subvolume_get_path("/", 0, &root_path);
    if (btrfs_err) {
        return std::nullopt;
    }

    QString result = QString::fromUtf8(root_path);
    free(root_path);
    return result;
}

std::optional<QString> getAbsoluteSubvolumePath(uint64_t subvolume)
{
    enum btrfs_util_error btrfs_err;

    char *path;
    btrfs_err = btrfs_util_subvolume_get_path("/", subvolume, &path);
    if (btrfs_err) {
        return std::nullopt;
    }

    QString subvolumePath = QString::fromUtf8(path);
    free(path);

    std::optional<QString> rootPath = getRootSubvolumePath();
    if (!rootPath.has_value()) {
        return std::nullopt;
    }

    if (!subvolumePath.startsWith(rootPath.value())) {
        return std::nullopt;
    }

    QString subvolumePathAbs = subvolumePath.sliced(rootPath.value().length());
    return subvolumePathAbs;
}

std::optional<QList<SubvolumeSnapshotInfo>> getSnapshotsForSubvolume(uint64_t subvolume, uint userId)
{
    QList<SubvolumeSnapshotInfo> snapshots;
    enum btrfs_util_error btrfs_err;

    std::optional<QString> subvolumePathAbs = getAbsoluteSubvolumePath(subvolume);
    if (!subvolumePathAbs.has_value()) {
        qCDebug(KSNAPSHOTSERVICE_LOG) << "couldn't get abs path for" << subvolume;
        return std::nullopt;
    }

    int subvolumeDirFd = openDirForUser(subvolumePathAbs.value(), userId);
    if (subvolumeDirFd == -1) {
        qCDebug(KSNAPSHOTSERVICE_LOG) << "couldn't open" << subvolumePathAbs.value() << "for" << userId;
        return std::nullopt;
    }
    close(subvolumeDirFd);

    struct btrfs_util_subvolume_info info;
    btrfs_err = btrfs_util_subvolume_get_info("/", subvolume, &info);
    if (btrfs_err) {
        qCDebug(KSNAPSHOTSERVICE_LOG) << btrfs_util_strerror(btrfs_err);
        return std::nullopt;
    }
    QByteArrayView subvolumeUuid = QByteArrayView::fromArray(info.uuid);

    struct btrfs_util_subvolume_iterator *it;
    btrfs_err = btrfs_util_subvolume_iter_create("/", subvolume, 0, &it);
    if (btrfs_err) {
        qCDebug(KSNAPSHOTSERVICE_LOG) << btrfs_util_strerror(btrfs_err);
        return std::nullopt;
    }

    struct btrfs_util_subvolume_info iter_info;
    char *iter_path;
    while (!(btrfs_err = btrfs_util_subvolume_iter_next_info(it, &iter_path, &iter_info))) {
        free(iter_path);
        if (QByteArrayView::fromArray(iter_info.parent_uuid) == subvolumeUuid) {
            // (parent_uuid == subvolume_uuid) => this is a snapshot of the subvolume
            std::optional<QString> snapshotPathOpt = ::getAbsoluteSubvolumePath(iter_info.id);
            if (!snapshotPathOpt.has_value()) {
                continue;
            }
            QString snapshotPath = snapshotPathOpt.value();
            int snapshotDirFd = openDirForUser(snapshotPath, userId);
            if (snapshotDirFd != -1) {
                close(snapshotDirFd);
                // user owns snapshot dir, so we can reveal it
                SubvolumeSnapshotInfo snapshot;
                snapshot.subvolumeId = static_cast<qulonglong>(iter_info.id);
                snapshot.path = snapshotPath;
                snapshot.snapshotTimeSecs = static_cast<qulonglong>(iter_info.otime.tv_sec);
                snapshot.snapshotTimeNanosecs = static_cast<qulonglong>(iter_info.otime.tv_nsec);

                snapshots << snapshot;
            }
        }
    }
    btrfs_util_subvolume_iter_destroy(it);

    return snapshots;
}

KSnapshotService::KSnapshotService(QObject *parent)
    : QObject(parent)
{
}

QDBusReply<uint> KSnapshotService::getUserId()
{
    QDBusConnection conn = connection();
    QDBusMessage msg = message();
    const QString &service = msg.service();
    return conn.interface()->serviceUid(service);
}

qulonglong KSnapshotService::getSubvolumeForPath(const QString &path)
{
    QDBusReply<uint> userIdReply = getUserId();
    if (!userIdReply.isValid()) {
        sendErrorReply(QDBusError::AccessDenied);
        return 0;
    }
    uint userId = userIdReply.value();

    int dirFd = openDirForUser(path, userId);
    if (dirFd == -1) {
        sendErrorReply(QDBusError::AccessDenied);
        return 0;
    }

    uint64_t subvolumeId;
    enum btrfs_util_error btrfs_err = btrfs_util_subvolume_get_id_fd(dirFd, &subvolumeId);
    if (btrfs_err) {
        sendErrorReply(QDBusError::InternalError, QString::fromUtf8(btrfs_util_strerror(btrfs_err)));
    }
    close(dirFd);
    return subvolumeId;
}

QString KSnapshotService::getPathForSubvolume(qulonglong subvolume)
{
    QDBusReply<uint> userIdReply = getUserId();
    if (!userIdReply.isValid()) {
        sendErrorReply(QDBusError::AccessDenied);
        return QString();
    }
    uint userId = userIdReply.value();

    std::optional<QString> subvolumePathAbs = getAbsoluteSubvolumePath(subvolume);

    if (!subvolumePathAbs.has_value()) {
        sendErrorReply(QDBusError::InternalError);
        return QString();
    }

    int dirFd = openDirForUser(subvolumePathAbs.value(), userId);
    if (dirFd == -1) {
        sendErrorReply(QDBusError::AccessDenied);
        return QString();
    }
    close(dirFd);

    return subvolumePathAbs.value();
}

QList<SubvolumeSnapshotInfo> KSnapshotService::getSnapshotsForSubvolume(qulonglong subvolume)
{
    QDBusReply<uint> userIdReply = getUserId();
    if (!userIdReply.isValid()) {
        qCDebug(KSNAPSHOTSERVICE_LOG) << "couldn't get user ID, denying";
        sendErrorReply(QDBusError::AccessDenied);
        return QList<SubvolumeSnapshotInfo>();
    }
    uint userId = userIdReply.value();

    std::optional<QList<SubvolumeSnapshotInfo>> snapshots = ::getSnapshotsForSubvolume(subvolume, userId);
    if (!snapshots.has_value()) {
        qCDebug(KSNAPSHOTSERVICE_LOG) << "getSnapshotsForSubvolume failed internally, denying";
        sendErrorReply(QDBusError::AccessDenied);
        return QList<SubvolumeSnapshotInfo>();
    }

    return snapshots.value();
}

QList<FileSnapshotInfo> KSnapshotService::getSnapshotsForFile(const QString &path)
{
    QDBusReply<uint> userIdReply = getUserId();
    if (!userIdReply.isValid()) {
        sendErrorReply(QDBusError::AccessDenied);
        return QList<FileSnapshotInfo>();
    }
    uint userId = userIdReply.value();

    QFileInfo fileInfo(path);
    QDir parentDir = fileInfo.absoluteDir();

    if (!fileInfo.exists() || fileInfo.ownerId() != userId) {
        sendErrorReply(QDBusError::AccessDenied);
        return QList<FileSnapshotInfo>();
    }

    int parentDirFd = openDirForUser(parentDir.absolutePath(), userId);
    if (parentDirFd == -1) {
        sendErrorReply(QDBusError::AccessDenied);
        return QList<FileSnapshotInfo>();
    }
    uint64_t subvolume;
    enum btrfs_util_error btrfs_err = btrfs_util_subvolume_get_id_fd(parentDirFd, &subvolume);
    if (btrfs_err) {
        sendErrorReply(QDBusError::AccessDenied);
    }
    close(parentDirFd);

    std::optional<QString> subvolumeRootOpt = ::getAbsoluteSubvolumePath(subvolume);
    if (!subvolumeRootOpt.has_value()) {
        sendErrorReply(QDBusError::AccessDenied);
        return QList<FileSnapshotInfo>();
    }
    QDir subvolumeRoot(subvolumeRootOpt.value());

    QString pathRel = subvolumeRoot.relativeFilePath(path);

    std::optional<QList<SubvolumeSnapshotInfo>> snapshotsOpt = ::getSnapshotsForSubvolume(subvolume, userId);
    if (!snapshotsOpt.has_value()) {
        sendErrorReply(QDBusError::AccessDenied);
        return QList<FileSnapshotInfo>();
    }
    QList<SubvolumeSnapshotInfo> snapshots = snapshotsOpt.value();

    QList<FileSnapshotInfo> fileSnapshots;
    for (const SubvolumeSnapshotInfo &snapshot : snapshots) {
        FileSnapshotInfo fileSnapshot;
        QString fileSnapshotPath = QDir(snapshot.path).absoluteFilePath(pathRel);
        int fd = open(fileSnapshotPath.toUtf8().constData(), O_RDONLY);
        struct stat sb;
        if (fd == -1 || fstat(fd, &sb) == -1 || sb.st_uid != userId) {
            if (fd != -1) {
                close(fd);
            }
            continue;
        }
        unsigned int generation;
        int ioctl_ret = ioctl(fd, FS_IOC_GETVERSION, &generation);
        if (ioctl_ret == -1) {
            qCDebug(KSNAPSHOTSERVICE_LOG()) << "FS_IOC_GETVERSION ioctl on" << fileSnapshotPath << "returned" << ioctl_ret << std::strerror(errno);
        }
        close(fd);
        fileSnapshot.path = fileSnapshotPath;
        fileSnapshot.snapshotTimeSecs = snapshot.snapshotTimeSecs;
        fileSnapshot.snapshotTimeNanosecs = snapshot.snapshotTimeNanosecs;
        fileSnapshot.modificationTimeSecs = static_cast<qulonglong>(sb.st_mtim.tv_sec);
        fileSnapshot.modificationTimeNanosecs = static_cast<qulonglong>(sb.st_mtim.tv_nsec);
        if (ioctl_ret == 0) {
            fileSnapshot.generation = static_cast<qulonglong>(generation);
        }
        fileSnapshots << fileSnapshot;
    }

    // the last entry is the queried file itself
    // this makes it convenient for the consumer to determine (by means of the Generation number) if the snapshots are actually modified wrt current file or not
    FileSnapshotInfo currentInfo;
    int fd = open(path.toUtf8().constData(), O_RDONLY);
    struct stat sb;
    if (fd != -1 && fstat(fd, &sb) != -1 && sb.st_uid == userId) {
        currentInfo.path = path;
        currentInfo.modificationTimeSecs = static_cast<qulonglong>(sb.st_mtim.tv_sec);
        currentInfo.modificationTimeNanosecs = static_cast<qulonglong>(sb.st_mtim.tv_nsec);
        currentInfo.snapshotTimeSecs = std::nullopt;
        currentInfo.snapshotTimeNanosecs = std::nullopt;
        unsigned int generation;
        int ioctl_ret = ioctl(fd, FS_IOC_GETVERSION, &generation);
        if (ioctl_ret == -1) {
            qCDebug(KSNAPSHOTSERVICE_LOG()) << "FS_IOC_GETVERSION ioctl on" << path << "returned" << ioctl_ret << std::strerror(errno);
        } else {
            currentInfo.generation = static_cast<qulonglong>(generation);
        }
        fileSnapshots << currentInfo;
    }

    return fileSnapshots;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qDBusRegisterMetaType<FileSnapshotInfo>();
    qDBusRegisterMetaType<QList<FileSnapshotInfo>>();
    qDBusRegisterMetaType<SubvolumeSnapshotInfo>();
    qDBusRegisterMetaType<QList<SubvolumeSnapshotInfo>>();

    QDBusConnection bus = QDBusConnection::systemBus();

    if (!bus.registerService("org.kde.ksnapshotservice"_L1)) {
        qCCritical(KSNAPSHOTSERVICE_LOG) << "Failed to register D-Bus service name:" << bus.lastError().message();
        return 1;
    }

    KSnapshotService service;

    if (!bus.registerObject("/KSnapshotService"_L1, &service, QDBusConnection::ExportScriptableContents)) {
        qCCritical(KSNAPSHOTSERVICE_LOG) << "Failed to export object on path:" << bus.lastError().message();
        return 1;
    }

    qCInfo(KSNAPSHOTSERVICE_LOG) << "KSnapshotService is running on D-Bus...";
    return app.exec();
}
