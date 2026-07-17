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

using namespace Qt::StringLiterals;

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

std::optional<qulonglong> getSubvolumeForPath(const QString &path, const QString &fsRoot = "/"_L1);
std::optional<QString> getPathForSubvolume(qulonglong subvolume, const QString &fsRoot = "/"_L1);
QList<SubvolumeSnapshot> getSnapshotsForSubvolume(const QString &path, const QString &fsRoot = "/"_L1);
QList<FileSnapshot> getSnapshotsForFile(const QString &path, const QString &fsRoot = "/"_L1);
QMap<qulonglong, QString> getNonSnapshotSubvolumes(const QString &fsRoot = "/"_L1);
}

#endif
