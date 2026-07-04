/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <KFormat>
#include <KIO/WorkerBase>

class FileSnapshotsProtocol : public QObject, public KIO::WorkerBase
{
    Q_OBJECT
public:
    FileSnapshotsProtocol(const QByteArray &pool, const QByteArray &app);
    ~FileSnapshotsProtocol() override;

    KIO::WorkerResult listDir(const QUrl &url) override;
    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult mimetype(const QUrl &url) override;

private:
    KFormat fmt;
};

#endif
