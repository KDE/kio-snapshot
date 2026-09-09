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

bool isOnBtrfs(const QString &fsPath);
std::optional<QUuid> getFsUuid(const QString &fsPath);
std::optional<QString> getFsRoot(const QString &fsPath);
QList<QString> getBtrfsSubvolMounts(QUuid fsUuid);
std::optional<qulonglong> getSubvolumeForPath(const QString &path);
std::optional<QString> getPathForSubvolume(qulonglong subvolume, QUuid fsUuid);
QList<SubvolumeSnapshot> getSnapshotsForSubvolume(const QString &path, QUuid fsUuid);
bool hasSnapshots(const QString &path, QUuid fsUuid);
QList<FileSnapshot> getSnapshotsForFile(const QString &path, QUuid fsUuid);
std::optional<QString> getOriginalForFileSnapshot(const QString &fileSnapshotPath, QUuid fsUuid);
QMap<qulonglong, QString> getNonSnapshotSubvolumes(QUuid fsUuid);
}

#endif
