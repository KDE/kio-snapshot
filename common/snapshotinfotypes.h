/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SNAPSHOTINFOTYPES_H
#define SNAPSHOTINFOTYPES_H

#include <QDBusMetaType>
#include <QList>
#include <QString>

using namespace Qt::StringLiterals;

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

Q_DECLARE_METATYPE(FileSnapshotInfo);
Q_DECLARE_METATYPE(QList<FileSnapshotInfo>);

inline QDBusArgument &operator<<(QDBusArgument &argument, const FileSnapshotInfo &data)
{
    QVariantMap map;

    map["Path"_L1] = data.path;
    map["Generation"_L1] = data.generation;
    map["ModificationTimeSec"_L1] = data.modificationTimeSecs;
    map["ModificationTimeNanosec"_L1] = data.modificationTimeNanosecs;
    if (data.snapshotTimeSecs.has_value()) {
        map["SnapshotCreationTimeSec"_L1] = data.snapshotTimeSecs.value();
    }
    if (data.snapshotTimeNanosecs.has_value()) {
        map["SnapshotCreationTimeNanosec"_L1] = data.snapshotTimeNanosecs.value();
    }

    argument << map;
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, FileSnapshotInfo &data)
{
    QVariantMap map;
    argument >> map;

    data.path = map.value("Path"_L1).toString();
    data.generation = map.value("Generation"_L1).toULongLong();
    data.modificationTimeSecs = map.value("ModificationTimeSec"_L1).toULongLong();
    data.modificationTimeNanosecs = map.value("ModificationTimeNanosec"_L1).toULongLong();
    if (map.contains("SnapshotCreationTimeSec"_L1)) {
        data.snapshotTimeSecs = map.value("SnapshotCreationTimeSec"_L1).toULongLong();
    } else {
        data.snapshotTimeSecs = std::nullopt;
    }
    if (map.contains("SnapshotCreationTimeNanosec"_L1)) {
        data.snapshotTimeNanosecs = map.value("SnapshotCreationTimeNanosec"_L1).toULongLong();
    } else {
        data.snapshotTimeNanosecs = std::nullopt;
    }

    return argument;
}

class SubvolumeSnapshotInfo
{
public:
    QString path;
    qulonglong subvolumeId;
    qulonglong snapshotTimeSecs;
    qulonglong snapshotTimeNanosecs;
};

Q_DECLARE_METATYPE(SubvolumeSnapshotInfo);
Q_DECLARE_METATYPE(QList<SubvolumeSnapshotInfo>);

inline QDBusArgument &operator<<(QDBusArgument &argument, const SubvolumeSnapshotInfo &data)
{
    QVariantMap map;

    map["Path"_L1] = data.path;
    map["SubvolumeId"_L1] = data.subvolumeId;
    map["SnapshotCreationTimeSec"_L1] = data.snapshotTimeSecs;
    map["SnapshotCreationTimeNanosec"_L1] = data.snapshotTimeNanosecs;

    argument << map;
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, SubvolumeSnapshotInfo &data)
{
    QVariantMap map;
    argument >> map;

    data.path = map.value("Path"_L1).toString();
    data.subvolumeId = map.value("SubvolumeId"_L1).toULongLong();
    data.snapshotTimeSecs = map.value("SnapshotCreationTimeSec"_L1).toULongLong();
    data.snapshotTimeNanosecs = map.value("SnapshotCreationTimeNanosec"_L1).toULongLong();

    return argument;
}

#endif
