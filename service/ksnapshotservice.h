/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "../common/snapshotinfotypes.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusReply>
#include <QObject>

#pragma once

class KSnapshotService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.ksnapshotservice")

public:
    KSnapshotService(QObject *parent = nullptr);

public Q_SLOTS:
    Q_SCRIPTABLE qulonglong getSubvolumeForPath(const QString &path);
    Q_SCRIPTABLE QString getPathForSubvolume(qulonglong subvolume);
    Q_SCRIPTABLE QList<SubvolumeSnapshotInfo> getSnapshotsForSubvolume(qulonglong subvolume);
    Q_SCRIPTABLE QList<FileSnapshotInfo> getSnapshotsForFile(const QString &path);

private:
    QDBusReply<uint> getUserId();
};
