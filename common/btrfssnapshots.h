/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>

#ifndef BTRFSSNAPSHOTS_H
#define BTRFSSNAPSHOTS_H

namespace BtrfsSnapshots
{
class FileSnapshot
{
public:
    QString path;
    qulonglong subvolumeId;
    QDateTime snapshotted;
    QDateTime modified;
};

class SubvolumeSnapshot
{
public:
    QString path;
    qulonglong subvolumeId;
    QDateTime snapshotted;
};

std::optional<qulonglong> getSubvolumeForPath(const QString &path);
std::optional<QString> getPathForSubvolume(qulonglong subvolume);
QList<SubvolumeSnapshot> getSnapshotsForSubvolume(const QString &path);
QList<FileSnapshot> getSnapshotsForFile(const QString &path);
QMap<qulonglong, QString> getNonSnapshotSubvolumes();
}

#endif
