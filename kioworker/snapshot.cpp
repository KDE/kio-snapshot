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
    if (snapshotUrl.kind() != SnapshotUrlKind::Subvolume) {
        return false;
    }
    auto snapshotId = snapshotUrl.snapshotId();
    if (!snapshotId.has_value()) {
        return false;
    }
    auto snapshotPathOpt = BtrfsSnapshots::getPathForSubvolume(snapshotId.value(), snapshotUrl.fsRoot());
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
    const auto kind = snapshotUrl.kind();

    if (kind == SnapshotUrlKind::Subvolume) {
        return listDirForSubvolume(snapshotUrl);
    } else if (kind == SnapshotUrlKind::File) {
        return listDirForFile(snapshotUrl);
    } else {
        return KIO::WorkerResult::fail(KIO::ERR_MALFORMED_URL);
    }
}

KIO::WorkerResult SnapshotProtocol::mimetype(const QUrl &url)
{
    const SnapshotUrl snapshotUrl(url);
    const auto kind = snapshotUrl.kind();

    if (kind == SnapshotUrlKind::Subvolume) {
        return mimetypeForSubvolume(snapshotUrl);
    } else if (kind == SnapshotUrlKind::File) {
        return mimetypeForFile(snapshotUrl);
    } else {
        return KIO::WorkerResult::fail(KIO::ERR_MALFORMED_URL);
    }
}

KIO::WorkerResult SnapshotProtocol::stat(const QUrl &url)
{
    const SnapshotUrl snapshotUrl(url);
    const auto kind = snapshotUrl.kind();

    if (kind == SnapshotUrlKind::Subvolume) {
        return statForSubvolume(snapshotUrl);
    } else if (kind == SnapshotUrlKind::File) {
        return statForFile(snapshotUrl);
    } else {
        return KIO::WorkerResult::fail(KIO::ERR_MALFORMED_URL);
    }
}

#include "snapshot.moc"
