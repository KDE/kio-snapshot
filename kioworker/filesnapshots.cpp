/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "filesnapshots.h"
#include "filesnapshots_debug.h"

#include <btrfssnapshots.h>

#include <KIO/Global>
#include <KIO/StatJob>
#include <KIO/UDSEntry>
#include <KIO/WorkerBase>

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
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.filesnapshots" FILE "filesnapshots.json")
};

extern "C" int Q_DECL_EXPORT kdemain(int argc, char **argv)
{
    // necessary to use other kio workers
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kio_filesnapshots"));
    if (argc != 4) {
        fprintf(stderr, "Usage: kio_filesnapshots protocol domain-socket1 domain-socket2\n");
        exit(-1);
    }
    // start the worker
    FileSnapshotsProtocol worker(argv[2], argv[3]);
    worker.dispatchLoop();
    return 0;
}

FileSnapshotsProtocol::FileSnapshotsProtocol(const QByteArray &pool, const QByteArray &app)
    : WorkerBase("filesnapshots", pool, app)
{
}

FileSnapshotsProtocol::~FileSnapshotsProtocol()
{
}

KIO::WorkerResult FileSnapshotsProtocol::listDir(const QUrl &url)
{
    qCDebug(KIO_FILESNAPSHOTS) << "url" << url << "url.host" << url.host() << "subvolume" << url.path();

    QList<BtrfsSnapshots::FileSnapshot> snapshots = BtrfsSnapshots::getSnapshotsForFile(url.path());

    std::sort(snapshots.begin(), snapshots.end(), [](const BtrfsSnapshots::FileSnapshot &a, const BtrfsSnapshots::FileSnapshot &b) {
        return a.snapshotted.toSecsSinceEpoch() > b.snapshotted.toSecsSinceEpoch();
    });

    QFileInfo currentInfo(url.path());
    BtrfsSnapshots::FileSnapshot current;
    current.path = url.path();
    current.snapshotted = currentInfo.lastModified();
    current.modified = currentInfo.lastModified();
    snapshots.insert(0, current);

    QList<BtrfsSnapshots::FileSnapshot> snapshotsFiltered;
    for (qsizetype i = 0; i < snapshots.size(); i++) {
        const BtrfsSnapshots::FileSnapshot &info = snapshots.at(i);
        if (i == 0 || snapshots.at(i - 1).modified != info.modified || snapshots.at(i - 1).modified != info.modified) {
            snapshotsFiltered << info;
        }
    }

    KIO::UDSEntryList udsList;
    for (const BtrfsSnapshots::FileSnapshot &snapshot : std::as_const(snapshotsFiltered)) {
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
            continue;
        }
        entry.replace(KIO::UDSEntry::UDS_NAME, "%1-%2"_L1.arg(entry.stringValue(KIO::UDSEntry::UDS_NAME), QString::number(snapshot.subvolumeId)));
        entry.replace(KIO::UDSEntry::UDS_ACCESS, S_IRUSR);
        if (snapshot.path == url.path()) {
            dateRepr = i18nc("denoting the present / most-recent version of the file", "Current");
        } else {
            dateRepr = fmt.formatRelativeDateTime(snapshot.snapshotted, QLocale::LongFormat);
        }
        entry.replace(KIO::UDSEntry::UDS_CREATION_TIME, snapshot.snapshotted.toSecsSinceEpoch());
        entry.replace(KIO::UDSEntry::UDS_MODIFICATION_TIME, snapshot.modified.toSecsSinceEpoch());
        entry.replace(KIO::UDSEntry::UDS_DISPLAY_NAME, dateRepr);
        entry.replace(KIO::UDSEntry::UDS_LOCAL_PATH, snapshot.path);
        entry.replace(KIO::UDSEntry::UDS_TARGET_URL, targetUrl.toString(QUrl::FullyEncoded));
        qCDebug(KIO_FILESNAPSHOTS) << entry;
        udsList << entry;
    }
    listEntries(udsList);

    return KIO::WorkerResult::pass();
}

KIO::WorkerResult FileSnapshotsProtocol::mimetype(const QUrl &url)
{
    (void)url;
    mimeType("inode/directory"_L1);
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult FileSnapshotsProtocol::stat(const QUrl &url)
{
    qCDebug(KIO_FILESNAPSHOTS) << "stat" << url;

    if (!url.path().isEmpty()) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST);
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

#include "filesnapshots.moc"
