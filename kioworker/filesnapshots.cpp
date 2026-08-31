/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "snapshot.h"
#include "snapshot_debug.h"

#include <btrfssnapshots.h>

#include <KIO/Global>
#include <KIO/ListJob>
#include <KIO/StatJob>
#include <KIO/UDSEntry>
#include <KIO/WorkerBase>

#include <Solid/Device>
#include <Solid/StorageAccess>

#include <KFormat>
#include <KLocalizedString>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLocale>
#include <QUrl>

std::optional<KIO::UDSEntry> statForSnapshot(const BtrfsSnapshots::FileSnapshot &snapshot)
{
    QUrl targetUrl = QUrl::fromLocalFile(snapshot.path);
    QString dateRepr;
    KIO::UDSEntry entry;
    KIO::StatJob *job = KIO::stat(targetUrl, KIO::HideProgressInfo);
    job->setDetails(KIO::StatDefaultDetails | KIO::StatSubVolId);
    QScopedPointer<KIO::StatJob> sp(job);
    sp->setAutoDelete(false);
    if (sp->exec()) {
        entry = sp->statResult();
    } else {
        return std::nullopt;
    }
    entry.replace(KIO::UDSEntry::UDS_NAME, "snapshot-%1-%2"_L1.arg(entry.stringValue(KIO::UDSEntry::UDS_NAME), QString::number(snapshot.subvolumeId)));
    entry.replace(KIO::UDSEntry::UDS_ACCESS, S_IRUSR);
    if (snapshot.subvolumeId == 0) {
        dateRepr = i18nc("denoting the present / most-recent version of the file", "Current");
    } else {
        dateRepr = KFormat().formatRelativeDateTime(snapshot.snapshotted, QLocale::LongFormat);
    }
    entry.replace(KIO::UDSEntry::UDS_CREATION_TIME, snapshot.snapshotted.toSecsSinceEpoch());
    entry.replace(KIO::UDSEntry::UDS_MODIFICATION_TIME, snapshot.modified.toSecsSinceEpoch());
    entry.replace(KIO::UDSEntry::UDS_DISPLAY_NAME, dateRepr);
    entry.replace(KIO::UDSEntry::UDS_LOCAL_PATH, snapshot.path);
    entry.replace(KIO::UDSEntry::UDS_TARGET_URL, targetUrl.toString(QUrl::FullyEncoded));

    return entry;
}

std::optional<BtrfsSnapshots::FileSnapshot> snapshotForWorkerUrl(const SnapshotUrl &workerUrl)
{
    if (workerUrl.fileName().startsWith("snapshot-"_L1) && !QFileInfo::exists(workerUrl.actualPath())) {
        bool gotSubvolId = false;
        qulonglong subvolId = workerUrl.fileName().section('-'_L1, -1).toULongLong(&gotSubvolId);
        if (!gotSubvolId) {
            return std::nullopt;
        }

        QUrl localUrl = QUrl::fromLocalFile(workerUrl.actualPath()).adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);

        if (subvolId == 0) {
            QFileInfo currentInfo(localUrl.path());
            BtrfsSnapshots::FileSnapshot current;
            current.path = localUrl.path();
            current.snapshotted = QDateTime::currentDateTime();
            current.modified = currentInfo.lastModified();
            current.subvolumeId = 0;
            return current;
        } else {
            auto fsRoot = Solid::Device::storageAccessFromPath(localUrl.path()).as<Solid::StorageAccess>();
            if (!fsRoot) {
                qCCritical(KIO_SNAPSHOT) << "could not determine fs root path for" << localUrl;
                return std::nullopt;
            }
            QString fsRootPath = fsRoot->filePath();

            const QList<BtrfsSnapshots::FileSnapshot> snapshots = BtrfsSnapshots::getSnapshotsForFile(localUrl.path(), fsRootPath);
            for (const auto &snapshot : snapshots) {
                if (snapshot.subvolumeId == subvolId) {
                    return snapshot;
                }
            }
        }
    }

    return std::nullopt;
}

KIO::WorkerResult SnapshotProtocol::listDirForFile(const SnapshotUrl &url)
{
    QString localPath = url.actualPath();
    auto fsRoot = Solid::Device::storageAccessFromPath(localPath).as<Solid::StorageAccess>();
    if (!fsRoot) {
        qCCritical(KIO_SNAPSHOT) << "could not determine fs root path for" << localPath;
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST);
    }
    QString fsRootPath = fsRoot->filePath();

    QList<BtrfsSnapshots::FileSnapshot> snapshots = BtrfsSnapshots::getSnapshotsForFile(url.actualPath(), fsRootPath);

    std::sort(snapshots.begin(), snapshots.end(), [](const BtrfsSnapshots::FileSnapshot &a, const BtrfsSnapshots::FileSnapshot &b) {
        return a.snapshotted.toMSecsSinceEpoch() > b.snapshotted.toMSecsSinceEpoch();
    });

    QFileInfo currentInfo(url.actualPath());
    BtrfsSnapshots::FileSnapshot current;
    current.path = url.actualPath();
    current.snapshotted = QDateTime::currentDateTime();
    current.modified = currentInfo.lastModified();
    current.subvolumeId = 0;
    snapshots.insert(0, current);

    QList<BtrfsSnapshots::FileSnapshot> snapshotsFiltered;
    for (qsizetype i = 0; i < snapshots.size(); i++) {
        const BtrfsSnapshots::FileSnapshot &info = snapshots.at(i);
        if (currentInfo.isDir() || i == 0 || i == snapshots.size() - 1 || snapshots.at(i + 1).modified != info.modified) {
            snapshotsFiltered << info;
        }
    }

    KIO::UDSEntryList udsList;
    for (const BtrfsSnapshots::FileSnapshot &snapshot : std::as_const(snapshotsFiltered)) {
        const auto stat = statForSnapshot(snapshot);
        if (stat.has_value()) {
            udsList << stat.value();
        }
    }
    listEntries(udsList);

    return KIO::WorkerResult::pass();
}

KIO::WorkerResult SnapshotProtocol::mimetypeForFile(const SnapshotUrl &url)
{
    if (url.fileName().startsWith("snapshot-"_L1) && !QFileInfo::exists(url.actualPath())) {
        const auto snapshot = snapshotForWorkerUrl(url);

        if (!snapshot.has_value()) {
            return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST);
        }

        KIO::StatJob *job = KIO::stat(QUrl::fromLocalFile(snapshot.value().path), KIO::HideProgressInfo);
        job->setDetails(KIO::StatMimeType);
        QScopedPointer<KIO::StatJob> sp(job);
        sp->setAutoDelete(false);
        if (sp->exec()) {
            mimeType(sp->statResult().stringValue(KIO::UDSEntry::UDS_MIME_TYPE));
            return KIO::WorkerResult::pass();
        } else {
            return KIO::WorkerResult::fail(sp->error());
        }
    }
    mimeType("inode/directory"_L1);
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult SnapshotProtocol::statForFile(const SnapshotUrl &url)
{
    qCDebug(KIO_SNAPSHOT) << "stat" << url;
    qCDebug(KIO_SNAPSHOT) << url.fileName();

    if (url.fileName().startsWith("snapshot-"_L1) && !QFileInfo::exists(url.actualPath())) {
        const auto snapshot = snapshotForWorkerUrl(url);
        if (!snapshot.has_value()) {
            return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST);
        }

        const auto snapshotStat = statForSnapshot(snapshot.value());
        if (snapshotStat.has_value()) {
            statEntry(snapshotStat.value());
            return KIO::WorkerResult::pass();
        }

        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST);
    }

    if (!url.actualPath().isEmpty()) {
        KIO::UDSEntry uds;
        uds.reserve(6);
        uds.fastInsert(KIO::UDSEntry::UDS_NAME, url.fileName());
        if (QFileInfo(url.actualPath()).isDir()) {
            uds.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, url.fileName());
        } else {
            uds.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME,
                           i18nc("@title denoting that this directory shows a listing of snapshots for the path %1", "Snapshots for %1", url.fileName()));
        }
        uds.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, u"view-history"_s);
        uds.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        uds.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, u"inode/directory"_s);
        uds.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR);
        statEntry(uds);
        return KIO::WorkerResult::pass();
    }

    KIO::UDSEntry uds;
    uds.reserve(6);
    uds.fastInsert(KIO::UDSEntry::UDS_NAME, "file_snapshots"_L1);
    uds.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, i18nc("@title denoting that this directory shows a listing of snapshots", "File Snapshots"));
    uds.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, u"view-history"_s);
    uds.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    uds.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, u"inode/directory"_s);
    uds.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR);
    statEntry(uds);

    return KIO::WorkerResult::pass();
}
