/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "snapshot.h"
#include "snapshot_debug.h"

#include <btrfssnapshots.h>

#include <KIO/ForwardingWorkerBase>
#include <KIO/Global>
#include <KIO/UDSEntry>

#include <Solid/Device>
#include <Solid/StorageAccess>
#include <Solid/StorageVolume>

#include <KLocalizedString>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLocale>
#include <QUrl>
#include <QUuid>

KIO::WorkerResult SnapshotProtocol::listDirForSubvolume(const SnapshotUrl &url)
{
    qCDebug(KIO_SNAPSHOT) << "url" << url << "url.host" << url.host() << "subvolume" << url.subvolumeId() << "snapshotId" << url.snapshotId() << "actualPath"
                          << url.actualPath();

    const QString fsRoot = url.fsRoot();

    if (!url.subvolumeId().has_value()) {
        KIO::UDSEntryList udsList;
        for (const auto [id, path] : BtrfsSnapshots::getNonSnapshotSubvolumes(fsRoot).asKeyValueRange()) {
            if (!BtrfsSnapshots::getSnapshotsForSubvolume(path, fsRoot).empty()) {
                KIO::UDSEntry entry;
                entry.fastInsert(KIO::UDSEntry::UDS_NAME, QString::number(id));
                entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME,
                                 i18nc("@title denoting a listing of snapshots for a directory; %1 is the path to the directory", "Snapshots for %1", path));
                entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, "view-history"_L1);
                entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, QT_STAT_DIR);
                QUrl targetUrl = url;
                targetUrl.setPath("/subvolume/"_L1 + QString::number(id));
                entry.fastInsert(KIO::UDSEntry::UDS_URL, targetUrl.toString(QUrl::FullyEncoded));
                udsList << entry;
            }
        }
        listEntries(udsList);
        return KIO::WorkerResult::pass();
    }

    if (url.snapshotId().has_value()) {
        return KIO::ForwardingWorkerBase::listDir(url);
    }

    auto subvolumePathOpt = BtrfsSnapshots::getPathForSubvolume(url.subvolumeId().value(), fsRoot);
    if (!subvolumePathOpt.has_value()) {
        return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED);
    }

    const QList<BtrfsSnapshots::SubvolumeSnapshot> snapshots = BtrfsSnapshots::getSnapshotsForSubvolume(subvolumePathOpt.value(), fsRoot);

    KIO::UDSEntryList udsList;
    for (const auto &snapshot : snapshots) {
        snapshotInfoMap[snapshot.subvolumeId] = snapshot;
        QString dirName = i18nc("@title denoting a snapshot taken at a specific time; %1 is the timestamp",
                                "Snapshot at %1",
                                QLocale::system().toString(snapshot.snapshotted, QLocale::ShortFormat));

        KIO::UDSEntry entry;
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, QString::number(snapshot.subvolumeId));
        entry.fastInsert(KIO::UDSEntry::UDS_SUBVOL_ID, snapshot.subvolumeId);
        entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, dirName);
        entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, snapshot.snapshotted.toSecsSinceEpoch());
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, QT_STAT_DIR);
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, "inode/directory"_L1);
        // QUrl targetUrl = url;
        // targetUrl.setPath(QDir::cleanPath("/%1/%2/"_L1.arg(QString::number(url.subvolumeId().value())).arg(QString::number(snapshot.subvolumeId))));
        // entry.fastInsert(KIO::UDSEntry::UDS_TARGET_URL, targetUrl.toString(QUrl::FullyEncoded));
        qCDebug(KIO_SNAPSHOT) << entry;
        udsList << entry;
    }
    listEntries(udsList);

    return KIO::WorkerResult::pass();
}

KIO::WorkerResult SnapshotProtocol::mimetypeForSubvolume(const SnapshotUrl &url)
{
    if (url.snapshotId().has_value()) {
        return ForwardingWorkerBase::mimetype(url);
    }

    mimeType("inode/directory"_L1);
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult SnapshotProtocol::statForSubvolume(const SnapshotUrl &url)
{
    if (!url.subvolumeId().has_value()) {
        KIO::UDSEntry uds;
        uds.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, "inode/directory"_L1);
        statEntry(uds);
        return KIO::WorkerResult::pass();
    }

    qulonglong subvolumeId = url.subvolumeId().value();
    const QString fsRoot = url.fsRoot();

    if (url.snapshotId().has_value() && !url.actualPath().isEmpty()) {
        qCDebug(KIO_SNAPSHOT()) << "forwarding stat...";
        return KIO::ForwardingWorkerBase::stat(url);
    }

    if (url.snapshotId().has_value() && url.actualPath().isEmpty()) {
        qulonglong snapshotId = url.snapshotId().value();

        BtrfsSnapshots::SubvolumeSnapshot snapshotInfo;
        if (snapshotInfoMap.contains(snapshotId)) {
            snapshotInfo = snapshotInfoMap[snapshotId];
        } else {
            auto snapshotPathOpt = BtrfsSnapshots::getPathForSubvolume(subvolumeId, fsRoot);
            if (!snapshotPathOpt.has_value()) {
                return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED);
            }
            const auto snapshotQuery = BtrfsSnapshots::getSnapshotsForSubvolume(snapshotPathOpt.value(), fsRoot);
            for (const auto &snapshot : snapshotQuery) {
                snapshotInfoMap[snapshot.subvolumeId] = snapshot;
                if (snapshot.subvolumeId == snapshotId) {
                    snapshotInfo = snapshot;
                }
            }
        }

        QString dirName = i18nc("@title denoting a snapshot taken at a specific time; %1 is the timestamp",
                                "Snapshot at %1",
                                QLocale::system().toString(snapshotInfo.snapshotted, QLocale::ShortFormat));

        KIO::UDSEntry uds;
        uds.reserve(7);
        uds.fastInsert(KIO::UDSEntry::UDS_NAME, QString::number(snapshotId));
        uds.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, dirName);
        uds.fastInsert(KIO::UDSEntry::UDS_DISPLAY_TYPE, i18nc("denoting that this directory is a snapshot", "Snapshot"));
        uds.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, u"view-history"_s);
        uds.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        uds.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, u"inode/directory"_s);
        uds.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, snapshotInfo.snapshotted.toSecsSinceEpoch());

        statEntry(uds);
        return KIO::WorkerResult::pass();
    }

    auto snapshotPathOpt = BtrfsSnapshots::getPathForSubvolume(subvolumeId, fsRoot);
    if (!snapshotPathOpt.has_value()) {
        return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED);
    }
    QString subvolumePath = snapshotPathOpt.value();
    KIO::UDSEntry uds;
    uds.reserve(7);
    uds.fastInsert(KIO::UDSEntry::UDS_NAME, "."_L1);
    uds.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME,
                   i18nc("@title denoting a listing of snapshots for a directory; %1 is the path to the directory", "Snapshots for %1", subvolumePath));
    uds.fastInsert(KIO::UDSEntry::UDS_DISPLAY_TYPE, i18nc("denoting that this directory shows a listing of snapshot", "Snapshots"));
    uds.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, u"view-history"_s);
    uds.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    uds.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, u"inode/directory"_s);
    uds.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IWUSR);
    statEntry(uds);

    return KIO::WorkerResult::pass();
}
