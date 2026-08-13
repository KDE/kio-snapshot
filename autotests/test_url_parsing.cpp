/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "../kioworker/snapshoturl.h"

#include <QDir>
#include <QObject>
#include <QString>
#include <QTest>
#include <QtTest>

using namespace Qt::StringLiterals;

class TestUrlParsing : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testEmpty()
    {
        QUrl url("snapshot://"_L1);
        SnapshotUrl snapshotUrl(url);
        QVERIFY(!snapshotUrl.kind().has_value());
        QVERIFY(!snapshotUrl.subvolumeId().has_value());
        QVERIFY(!snapshotUrl.snapshotId().has_value());
    };

    void testSubvolume()
    {
        QUrl url("snapshot:///subvolume"_L1);
        SnapshotUrl snapshotUrl(url);
        QCOMPARE(snapshotUrl.kind(), SnapshotUrlKind::Subvolume);
        QVERIFY(!snapshotUrl.subvolumeId().has_value());
        QVERIFY(!snapshotUrl.snapshotId().has_value());
        QCOMPARE(snapshotUrl.actualPath(), "/"_L1);
    };

    void testSubvolumeId()
    {
        QUrl url("snapshot:///subvolume/5"_L1);
        SnapshotUrl snapshotUrl(url);
        QCOMPARE(snapshotUrl.kind(), SnapshotUrlKind::Subvolume);
        QCOMPARE(snapshotUrl.subvolumeId(), 5);
        QVERIFY(!snapshotUrl.snapshotId().has_value());
        QCOMPARE(snapshotUrl.actualPath(), "/"_L1);
    }

    void testSubvolumeAndSnapshotId()
    {
        QUrl url("snapshot:///subvolume/5/300"_L1);
        SnapshotUrl snapshotUrl(url);
        QCOMPARE(snapshotUrl.kind(), SnapshotUrlKind::Subvolume);
        QCOMPARE(snapshotUrl.subvolumeId(), 5);
        QCOMPARE(snapshotUrl.snapshotId(), 300);
        QCOMPARE(snapshotUrl.actualPath(), "/"_L1);
    }

    void testSubvolumeAndSnapshotIdAndPath()
    {
        QUrl url("snapshot:///subvolume/5/300/file.txt"_L1);
        SnapshotUrl snapshotUrl(url);
        QCOMPARE(snapshotUrl.kind(), SnapshotUrlKind::Subvolume);
        QCOMPARE(snapshotUrl.subvolumeId(), 5);
        QCOMPARE(snapshotUrl.snapshotId(), 300);
        QCOMPARE(snapshotUrl.actualPath(), "/file.txt"_L1);
    }

    void testFile()
    {
        QUrl url("snapshot:///file"_L1);
        SnapshotUrl snapshotUrl(url);
        QCOMPARE(snapshotUrl.kind(), SnapshotUrlKind::File);
        QVERIFY(!snapshotUrl.subvolumeId().has_value());
        QVERIFY(!snapshotUrl.snapshotId().has_value());
        QCOMPARE(snapshotUrl.actualPath(), "/"_L1);
    }

    void testFilePath()
    {
        QUrl url("snapshot:///file/home/me"_L1);
        SnapshotUrl snapshotUrl(url);
        QCOMPARE(snapshotUrl.kind(), SnapshotUrlKind::File);
        QVERIFY(!snapshotUrl.subvolumeId().has_value());
        QVERIFY(!snapshotUrl.snapshotId().has_value());
        QCOMPARE(snapshotUrl.actualPath(), "/home/me"_L1);
    }
};

QTEST_GUILESS_MAIN(TestUrlParsing)

#include "test_url_parsing.moc"
