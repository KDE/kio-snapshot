/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SNAPSHOTINFOTYPES_H
#define SNAPSHOTINFOTYPES_H

#include <QString>

class FileSnapshotInfo
{
public:
    QString path;
    qulonglong generation;
    std::optional<qulonglong> snapshotTimeSecs;
    std::optional<qulonglong> snapshotTimeNanosecs;
    qulonglong modificationTimeSecs;
    qulonglong modificationTimeNanosecs;
};

class SubvolumeSnapshotInfo
{
public:
    QString path;
    qulonglong subvolumeId;
    std::optional<qulonglong> snapshotTimeSecs;
    std::optional<qulonglong> snapshotTimeNanosecs;
};

#endif
