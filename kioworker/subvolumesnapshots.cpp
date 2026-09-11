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
    const QString fsRoot = url.fsRoot();

    if (!url.subvolumeId().has_value()) {
        KIO::UDSEntryList udsList;
        for (const auto [id, path] : BtrfsSnapshots::getNonSnapshotSubvolumes(fsRoot).asKeyValueRange()) {
            if (!BtrfsSnapshots::getSnapshotsForSubvolume(path, fsRoot).empty()) {
                const QString subvolumeId = QString::number(id);
                QUrl targetUrl = url;
                targetUrl.setPath("/subvolume/"_L1 + subvolumeId);

                KIO::UDSEntry entry;
                entry.insert({{KIO::UDSEntry::UDS_NAME, subvolumeId},
                              {KIO::UDSEntry::UDS_DISPLAY_NAME,
                               i18nc("@title denoting a listing of snapshots for a directory; %1 is the path to the directory", "Snapshots for %1", path)},
                              {KIO::UDSEntry::UDS_ICON_NAME, u"view-history"_s},
                              {KIO::UDSEntry::UDS_URL, targetUrl.toString(QUrl::FullyEncoded)}});
                entry.insert({{KIO::UDSEntry::UDS_FILE_TYPE, QT_STAT_DIR}});
                udsList << std::move(entry);
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
    udsList.reserve(snapshots.size());
    for (const auto &snapshot : snapshots) {
        snapshotInfoMap.insert(snapshot.subvolumeId, snapshot);
        const QString dirName = i18nc("@title denoting a snapshot taken at a specific time; %1 is the timestamp",
                                      "Snapshot at %1",
                                      QLocale::system().toString(snapshot.snapshotted, QLocale::ShortFormat));

        KIO::UDSEntry entry;
        entry.insert({{KIO::UDSEntry::UDS_NAME, QString::number(snapshot.subvolumeId)},
                      {KIO::UDSEntry::UDS_DISPLAY_NAME, dirName},
                      {KIO::UDSEntry::UDS_MIME_TYPE, u"inode/directory"_s}});
        entry.insert({{KIO::UDSEntry::UDS_SUBVOL_ID, snapshot.subvolumeId},
                      {KIO::UDSEntry::UDS_CREATION_TIME, snapshot.snapshotted.toSecsSinceEpoch()},
                      {KIO::UDSEntry::UDS_FILE_TYPE, QT_STAT_DIR}});
        udsList << std::move(entry);
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

    if (url.snapshotId().has_value() && url.actualPath() != "/"_L1) {
        return KIO::ForwardingWorkerBase::stat(url);
    }

    if (url.snapshotId().has_value() && url.actualPath() == "/"_L1) {
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
        uds.insert({{KIO::UDSEntry::UDS_NAME, QString::number(snapshotId)},
                    {KIO::UDSEntry::UDS_DISPLAY_NAME, dirName},
                    {KIO::UDSEntry::UDS_DISPLAY_TYPE, i18nc("denoting that this directory is a snapshot", "Snapshot")},
                    {KIO::UDSEntry::UDS_ICON_NAME, u"view-history"_s},
                    {KIO::UDSEntry::UDS_MIME_TYPE, u"inode/directory"_s}});
        uds.insert({{KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR}, {KIO::UDSEntry::UDS_CREATION_TIME, snapshotInfo.snapshotted.toSecsSinceEpoch()}});

        statEntry(uds);
        return KIO::WorkerResult::pass();
    }

    auto snapshotPathOpt = BtrfsSnapshots::getPathForSubvolume(subvolumeId, fsRoot);
    if (!snapshotPathOpt.has_value()) {
        return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED);
    }
    QString subvolumePath = snapshotPathOpt.value();
    KIO::UDSEntry uds;
    uds.insert({{KIO::UDSEntry::UDS_NAME, u"."_s},
                {KIO::UDSEntry::UDS_DISPLAY_NAME,
                 i18nc("@title denoting a listing of snapshots for a directory; %1 is the path to the directory", "Snapshots for %1", subvolumePath)},
                {KIO::UDSEntry::UDS_DISPLAY_TYPE, i18nc("denoting that this directory shows a listing of snapshot", "Snapshots")},
                {KIO::UDSEntry::UDS_ICON_NAME, u"view-history"_s},
                {KIO::UDSEntry::UDS_MIME_TYPE, u"inode/directory"_s}});
    uds.insert({{KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR}, {KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IWUSR}});
    statEntry(uds);

    return KIO::WorkerResult::pass();
}
