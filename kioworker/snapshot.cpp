/*
 *    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
 *
 *    SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "snapshot.h"
#include "debug.h"

#include "service_interface.h"

#include <KIO/ForwardingWorkerBase>
#include <KIO/Global>
#include <KIO/UDSEntry>

#include <KLocalizedString>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLocale>
#include <QUrl>
#include <kio/global.h>
#include <kio/udsentry.h>
#include <qdbuspendingreply.h>
#include <qloggingcategory.h>

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

    qulonglong subvolumeId() const
    {
        // TODO handle missing subvolume ID
        return host().replace("subvolume"_L1, ""_L1).toULongLong();
    }

    std::optional<qulonglong> snapshotId() const
    {
        bool ok;
        qulonglong id = path().section('/'_L1, 0, 0, QString::SectionFlag::SectionSkipEmpty).toULongLong(&ok);
        if (!ok) {
            return std::nullopt;
        }
        return id;
    }

    QString actualPath() const
    {
        return path().section('/'_L1, 1, -1, QString::SectionFlag::SectionSkipEmpty);
    }
};

SnapshotProtocol::SnapshotProtocol(const QByteArray &pool, const QByteArray &app)
    : ForwardingWorkerBase("snapshot", pool, app)
    , service(new org::kde::ksnapshotservice("org.kde.ksnapshotservice"_L1, "/KSnapshotService"_L1, QDBusConnection::systemBus(), this))
{
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kio_snapshot"));
}

SnapshotProtocol::~SnapshotProtocol()
{
}

bool SnapshotProtocol::rewriteUrl(const QUrl &url, QUrl &newUrl)
{
    const SnapshotUrl snapshotUrl(url);
    auto snapshotId = snapshotUrl.snapshotId();
    auto snapshotPathReply = service->getPathForSubvolume(snapshotId.value());
    snapshotPathReply.waitForFinished();
    if (!snapshotPathReply.isValid()) {
        warning(i18n("Could not open snapshot"));
        return false;
    }
    const QString &snapshotPath = snapshotPathReply.value();
    QString finalPath = QDir::cleanPath("/%1/%2"_L1.arg(snapshotPath).arg(snapshotUrl.actualPath()));
    newUrl = QUrl::fromLocalFile(finalPath);
    return true;
}

KIO::WorkerResult SnapshotProtocol::listDir(const QUrl &url)
{
    const SnapshotUrl snapshotUrl(url);

    qCDebug(KIO_SNAPSHOT) << "url" << url << "url.host" << url.host() << "subvolume" << snapshotUrl.subvolumeId() << "snapshotId" << snapshotUrl.snapshotId()
                          << "actualPath" << snapshotUrl.actualPath();

    if (snapshotUrl.snapshotId().has_value()) {
        return KIO::ForwardingWorkerBase::listDir(url);
    }

    auto snapshotQueryReply = service->getSnapshotsForSubvolume(snapshotUrl.subvolumeId());
    snapshotQueryReply.waitForFinished();
    if (!snapshotQueryReply.isValid()) {
        return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED);
    }

    QVariantList snapshots = snapshotQueryReply.value();
    KIO::UDSEntryList udsList;
    for (const QVariant &snapshotV : snapshots) {
        const QVariantMap &snapshot = qdbus_cast<QVariantMap>(snapshotV.value<QDBusArgument>());
        QDir snapshotDir(snapshot["Path"_L1].toString());
        qulonglong snapshotSubvolumeId = snapshot["SubvolumeId"_L1].toULongLong();
        qulonglong creationSecs = snapshot["CreationTimeSec"_L1].toULongLong();
        qulonglong creationNanosecs = snapshot["CreationTimeNanosec"_L1].toULongLong();
        QDateTime creation = QDateTime::fromSecsSinceEpoch(creationSecs);

        snapshotInfoMap[snapshotSubvolumeId] = snapshot;
        QString dirName = i18n("Snapshot at %1", QLocale::system().toString(creation, QLocale::ShortFormat));

        KIO::UDSEntry entry;
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, QString::number(snapshotSubvolumeId));
        entry.fastInsert(KIO::UDSEntry::UDS_SUBVOL_ID, snapshotSubvolumeId);
        entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, dirName);
        entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, creationSecs);
        entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME_NS_OFFSET, creationNanosecs);
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, "inode/directory"_L1);
        QUrl targetUrl;
        targetUrl.setScheme("snapshot"_L1);
        targetUrl.setHost(snapshotUrl.host());
        targetUrl.setPath(QDir::cleanPath("%1/"_L1.arg(QString::number(snapshotSubvolumeId))));
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

    if (snapshotUrl.snapshotId().has_value() && !snapshotUrl.actualPath().isEmpty()) {
        qCDebug(KIO_SNAPSHOT()) << "forwarding stat...";
        return KIO::ForwardingWorkerBase::stat(url);
    }

    if (snapshotUrl.snapshotId().has_value() && snapshotUrl.actualPath().isEmpty()) {
        qulonglong snapshotId = snapshotUrl.snapshotId().value();

        QVariantMap snapshotInfo;
        if (snapshotInfoMap.contains(snapshotId)) {
            snapshotInfo = snapshotInfoMap[snapshotId];
        } else {
            auto snapshotQueryReply = service->getSnapshotsForSubvolume(snapshotUrl.subvolumeId());
            snapshotQueryReply.waitForFinished();
            if (!snapshotQueryReply.isValid()) {
                return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED);
            }
            for (const QVariant &snapshotV : snapshotQueryReply.value()) {
                const QVariantMap &snapshotMap = qdbus_cast<QVariantMap>(snapshotV.value<QDBusArgument>());
                snapshotInfoMap[snapshotMap["SubvolumeId"_L1].toULongLong()] = snapshotMap;
                if (snapshotMap["SubvolumeId"_L1].toULongLong() == snapshotId) {
                    snapshotInfo = snapshotMap;
                }
            }
        }

        QDateTime creation = QDateTime::fromSecsSinceEpoch(snapshotInfo["CreationTimeSec"_L1].toULongLong());
        QString dirName = i18n("Snapshot at %1", QLocale::system().toString(creation, QLocale::ShortFormat));

        KIO::UDSEntry uds;
        uds.reserve(7);
        uds.fastInsert(KIO::UDSEntry::UDS_NAME, QString::number(snapshotId));
        uds.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, dirName);
        uds.fastInsert(KIO::UDSEntry::UDS_DISPLAY_TYPE, i18n("Snapshot"));
        uds.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, u"view-history"_s);
        uds.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        uds.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, u"inode/directory"_s);
        uds.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, snapshotInfo["CreationTimeSec"_L1].toULongLong());

        statEntry(uds);
        return KIO::WorkerResult::pass();
    }

    auto snapshotQueryReply = service->getPathForSubvolume(snapshotUrl.subvolumeId());
    snapshotQueryReply.waitForFinished();
    if (!snapshotQueryReply.isValid()) {
        return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED);
    }
    QString subvolumePath = snapshotQueryReply.value();
    KIO::UDSEntry uds;
    uds.reserve(7);
    uds.fastInsert(KIO::UDSEntry::UDS_NAME, QString::number(snapshotUrl.subvolumeId()));
    uds.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, i18n("Snapshots for %1", subvolumePath));
    uds.fastInsert(KIO::UDSEntry::UDS_DISPLAY_TYPE, i18n("Snapshots"));
    uds.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, u"view-history"_s);
    uds.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    uds.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, u"inode/directory"_s);
    uds.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR);
    statEntry(uds);

    return KIO::WorkerResult::pass();
}

#include "snapshot.moc"
