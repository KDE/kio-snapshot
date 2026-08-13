/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include "snapshoturl.h"

#include <KIO/ForwardingWorkerBase>
#include <KIO/WorkerBase>

#include <QUrl>

namespace BtrfsSnapshots
{
class SubvolumeSnapshot;
}

class SnapshotProtocol : public KIO::ForwardingWorkerBase
{
    Q_OBJECT
public:
    SnapshotProtocol(const QByteArray &pool, const QByteArray &app);
    ~SnapshotProtocol() override;

    KIO::WorkerResult listDir(const QUrl &url) override;
    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult mimetype(const QUrl &url) override;

protected:
    bool rewriteUrl(const QUrl &url, QUrl &newUrl) override;

private:
    QHash<qulonglong, BtrfsSnapshots::SubvolumeSnapshot> snapshotInfoMap;
    KIO::WorkerResult listDirForSubvolume(const SnapshotUrl &url);
    KIO::WorkerResult statForSubvolume(const SnapshotUrl &url);
    KIO::WorkerResult mimetypeForSubvolume(const SnapshotUrl &url);
    KIO::WorkerResult listDirForFile(const SnapshotUrl &url);
    KIO::WorkerResult statForFile(const SnapshotUrl &url);
    KIO::WorkerResult mimetypeForFile(const SnapshotUrl &url);
};

#endif
