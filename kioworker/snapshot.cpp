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

#include <KLocalizedString>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLocale>
#include <QUrl>

using namespace Qt::StringLiterals;

class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.snapshot" FILE "snapshot.json")
};

extern "C" int Q_DECL_EXPORT kdemain(int argc, char **argv)
{
    // necessary to use other kio workers
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kio_snapshot"));
    if (argc != 4) {
        fprintf(stderr, "Usage: kio_snapshot protocol domain-socket1 domain-socket2\n");
        exit(-1);
    }
    // start the worker
    SnapshotProtocol worker(argv[2], argv[3]);
    worker.dispatchLoop();
    return 0;
}

class SnapshotUrl : public QUrl
{
public:
    SnapshotUrl(const QUrl &url)
        : QUrl(url)
    {
    }

    std::optional<qulonglong> subvolumeId() const
    {
        bool ok;
        qulonglong id = path().section('/'_L1, 0, 0, QString::SectionFlag::SectionSkipEmpty).toULongLong(&ok);
        if (!ok) {
            return std::nullopt;
        }
        return id;
    }

    std::optional<qulonglong> snapshotId() const
    {
        bool ok;
        qulonglong id = path().section('/'_L1, 1, 1, QString::SectionFlag::SectionSkipEmpty).toULongLong(&ok);
        if (!ok) {
            return std::nullopt;
        }
        return id;
    }

    QString actualPath() const
    {
        return path().section('/'_L1, 2, -1, QString::SectionFlag::SectionSkipEmpty);
    }
};

SnapshotProtocol::SnapshotProtocol(const QByteArray &pool, const QByteArray &app)
    : ForwardingWorkerBase("snapshot", pool, app)
{
}

SnapshotProtocol::~SnapshotProtocol()
{
}

bool SnapshotProtocol::rewriteUrl(const QUrl &url, QUrl &newUrl)
{
    const SnapshotUrl snapshotUrl(url);
    auto snapshotId = snapshotUrl.snapshotId();
    auto snapshotPathOpt = BtrfsSnapshots::getPathForSubvolume(snapshotId.value());
    if (!snapshotPathOpt.has_value()) {
        warning(i18nc("@info warning", "Could not open snapshot"));
        return false;
    }
    const QString &snapshotPath = snapshotPathOpt.value();
    QString finalPath = QDir::cleanPath("/%1/%2"_L1.arg(snapshotPath).arg(snapshotUrl.actualPath()));
    newUrl = QUrl::fromLocalFile(finalPath);
    return true;
}

KIO::WorkerResult SnapshotProtocol::listDir(const QUrl &url)
{
    const SnapshotUrl snapshotUrl(url);

    qCDebug(KIO_SNAPSHOT) << "url" << url << "url.host" << url.host() << "subvolume" << snapshotUrl.subvolumeId() << "snapshotId" << snapshotUrl.snapshotId()
                          << "actualPath" << snapshotUrl.actualPath();

    if (!snapshotUrl.subvolumeId().has_value()) {
        KIO::UDSEntryList udsList;
        for (const auto [id, path] : BtrfsSnapshots::getNonSnapshotSubvolumes().asKeyValueRange()) {
            if (!BtrfsSnapshots::getSnapshotsForSubvolume(path).empty()) {
                KIO::UDSEntry entry;
                entry.fastInsert(KIO::UDSEntry::UDS_NAME, "subvolume%1"_L1.arg(QString::number(id)));
                entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME,
                                 i18nc("@title denoting a listing of snapshots for a directory; %1 is the path to the directory", "Snapshots for %1", path));
                entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, "view-history"_L1);
                entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, QT_STAT_DIR);
                entry.fastInsert(KIO::UDSEntry::UDS_URL, "snapshot:///%1"_L1.arg(QString::number(id)));
                udsList << entry;
            }
        }
        listEntries(udsList);
        return KIO::WorkerResult::pass();
    }

    if (snapshotUrl.snapshotId().has_value()) {
        return KIO::ForwardingWorkerBase::listDir(url);
    }

    auto subvolumePathOpt = BtrfsSnapshots::getPathForSubvolume(snapshotUrl.subvolumeId().value());
    if (!subvolumePathOpt.has_value()) {
        return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED);
    }

    QList<BtrfsSnapshots::SubvolumeSnapshot> snapshots = BtrfsSnapshots::getSnapshotsForSubvolume(subvolumePathOpt.value());

    KIO::UDSEntryList udsList;
    for (const auto &snapshot : snapshots) {
        QDir snapshotDir(snapshot.path);

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
        QUrl targetUrl;
        targetUrl.setScheme("snapshot"_L1);
        targetUrl.setPath(QDir::cleanPath("/%1/%2/"_L1.arg(QString::number(snapshotUrl.subvolumeId().value())).arg(QString::number(snapshot.subvolumeId))));
        entry.fastInsert(KIO::UDSEntry::UDS_TARGET_URL, targetUrl.toString(QUrl::FullyEncoded));
        qCDebug(KIO_SNAPSHOT) << entry;
        udsList << entry;
    }
    listEntries(udsList);

    return KIO::WorkerResult::pass();
}

KIO::WorkerResult SnapshotProtocol::mimetype(const QUrl &url)
{
    const SnapshotUrl snapshotUrl(url);
    if (snapshotUrl.snapshotId().has_value()) {
        return ForwardingWorkerBase::mimetype(url);
    }

    mimeType("inode/directory"_L1);
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult SnapshotProtocol::stat(const QUrl &url)
{
    qCDebug(KIO_SNAPSHOT()) << "stat" << url;

    const SnapshotUrl snapshotUrl(url);

    if (!snapshotUrl.subvolumeId().has_value()) {
        KIO::UDSEntry uds;
        uds.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, ""_L1);
        uds.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, "inode/directory"_L1);
        statEntry(uds);
        return KIO::WorkerResult::pass();
    }

    qulonglong subvolumeId = snapshotUrl.subvolumeId().value();

    if (snapshotUrl.snapshotId().has_value() && !snapshotUrl.actualPath().isEmpty()) {
        qCDebug(KIO_SNAPSHOT()) << "forwarding stat...";
        return KIO::ForwardingWorkerBase::stat(url);
    }

    if (snapshotUrl.snapshotId().has_value() && snapshotUrl.actualPath().isEmpty()) {
        qulonglong snapshotId = snapshotUrl.snapshotId().value();

        BtrfsSnapshots::SubvolumeSnapshot snapshotInfo;
        if (snapshotInfoMap.contains(snapshotId)) {
            snapshotInfo = snapshotInfoMap[snapshotId];
        } else {
            auto snapshotPathOpt = BtrfsSnapshots::getPathForSubvolume(subvolumeId);
            if (!snapshotPathOpt.has_value()) {
                return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED);
            }
            auto snapshotQuery = BtrfsSnapshots::getSnapshotsForSubvolume(snapshotPathOpt.value());
            for (const auto &snapshot : snapshotQuery) {
                snapshotInfoMap[snapshot.subvolumeId] = snapshot;
                if (snapshot.subvolumeId == snapshotId) {
                    snapshotInfo = snapshot;
                }
            }
        }

        QString dirName = i18nc("@title denoting a snapshot taken at a specific time - %1 is the timestamp",
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

    auto snapshotPathOpt = BtrfsSnapshots::getPathForSubvolume(subvolumeId);
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

#include "snapshot.moc"
