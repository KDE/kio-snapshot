/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "../common/btrfssnapshots.h"

#include <QDir>
#include <QObject>
#include <QString>
#include <QtTest>

using namespace Qt::StringLiterals;
using namespace BtrfsSnapshots;

// for some methods of BtrfsSnapshots that can't be exercised through the worker

class TestBtrfsSnapshotsCore : public QObject
{
    Q_OBJECT

private:
    QString m_testMount;
    QUuid m_testUuid;
    QString m_testComplexMount;
    QUuid m_testComplexUuid;

private Q_SLOTS:
    void initTestCase()
    {
        m_testMount = QString::fromUtf8(qgetenv("KIO_SNAPSHOT_TEST_MOUNTPOINT"));
        if (m_testMount.isEmpty() || !QDir(m_testMount).exists()) {
            QFAIL("Could not access mountpoint for test Btrfs filesystem (check env var KIO_SNAPSHOT_TEST_MOUNTPOINT)");
        }
        m_testUuid = getFsUuid(m_testMount).value_or(QUuid());
        if (m_testUuid.isNull()) {
            QFAIL("Could not determine FS UUID for test Btrfs filesystem");
        }

        m_testComplexMount = QString::fromUtf8(qgetenv("KIO_SNAPSHOT_TEST_COMPLEX_MOUNTPOINT"));
        if (m_testComplexMount.isEmpty() || !QDir(m_testComplexMount).exists()) {
            QFAIL("Could not access mountpoint for test Btrfs filesystem (check env var KIO_SNAPSHOT_TEST_COMPLEX_MOUNTPOINT)");
        }
        m_testComplexUuid = getFsUuid(m_testComplexMount).value_or(QUuid());
        if (m_testComplexUuid.isNull()) {
            QFAIL("Could not determine FS UUID for test complex Btrfs filesystem");
        }
    };

    void testRootFile()
    {
        const QString rootFile = QDir(m_testMount).absoluteFilePath("file.txt"_L1);
        const auto snapshots = getSnapshotsForFile(rootFile, m_testUuid);
        QCOMPARE(snapshots.size(), 4);
        for (const auto &snapshot : snapshots) {
            QCOMPARE(getOriginalForFileSnapshot(snapshot.path, m_testUuid), rootFile);
        }
    };

    void testSubvolumeFile()
    {
        const QString subvolumeFile = QDir(QDir::cleanPath(m_testMount + "/sub/"_L1)).absoluteFilePath("vol.txt"_L1);
        const auto snapshots = getSnapshotsForFile(subvolumeFile, m_testUuid);
        QCOMPARE(snapshots.size(), 4);
        for (const auto &snapshot : snapshots) {
            QCOMPARE(getOriginalForFileSnapshot(snapshot.path, m_testUuid), subvolumeFile);
        }
    };

    void testComplexFile()
    {
        const QString complexFile = QDir(m_testComplexMount).absoluteFilePath("data.txt"_L1);
        const auto snapshots = getSnapshotsForFile(complexFile, m_testComplexUuid);
        QCOMPARE(snapshots.size(), 1);
        for (const auto &snapshot : snapshots) {
            QCOMPARE(getOriginalForFileSnapshot(snapshot.path, m_testComplexUuid), complexFile);
        }
    }
};

QTEST_GUILESS_MAIN(TestBtrfsSnapshotsCore)

#include "test_btrfssnapshots_core.moc"
