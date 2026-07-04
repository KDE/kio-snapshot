/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <KIO/ForwardingWorkerBase>
#include <KIO/WorkerBase>

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
};

#endif
