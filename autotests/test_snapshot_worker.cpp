/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/ListJob>

#include <Solid/Device>
#include <Solid/StorageAccess>
#include <Solid/StorageVolume>

#include <QDir>
#include <QObject>
#include <QString>
#include <QTest>
#include <QtTest>

using namespace Qt::StringLiterals;

class TestSnapshotWorker : public QObject
{
    Q_OBJECT

private:
    QString m_testMount;
    QString m_fsUuid;
    QUrl m_subSnapshotsUrl;

private Q_SLOTS:
    void initTestCase()
    {
        m_testMount = QString::fromUtf8(qgetenv("KIO_SNAPSHOT_TEST_MOUNTPOINT"));
        if (m_testMount.isEmpty() || !QDir(m_testMount).exists()) {
            QFAIL("Could not access mountpoint for test Btrfs filesystem (check env var KIO_SNAPSHOT_TEST_MOUNTPOINT)");
        }
        auto fsDevice = Solid::Device::storageAccessFromPath(m_testMount);
        auto fsAccess = fsDevice.as<Solid::StorageAccess>();
        if (!fsAccess) {
            QFAIL("could not determine fs root path");
        }
        qDebug() << fsAccess->filePath();
        QString fsRootPath = fsAccess->filePath();
        auto fsVolume = fsDevice.as<Solid::StorageVolume>();
        if (!fsVolume) {
            QFAIL("could not determine fs storage volume for");
        }
        qDebug() << fsVolume->uuid();
        m_fsUuid = fsVolume->uuid();
    };

    void testListAllSubvolumes()
    {
        QUrl url;
        url.setScheme("snapshot"_L1);
        url.setHost(m_fsUuid);
        KIO::ListJob *listJob = KIO::listDir(url, KIO::HideProgressInfo);
        connect(listJob, &KIO::ListJob::entries, this, &TestSnapshotWorker::slotAllSubvolumesEntries);
        QVERIFY(listJob->exec());
    };

    void testSubvolume()
    {
        QUrl url;
        url.setScheme("snapshot"_L1);
        url.setHost(m_fsUuid);
        KIO::ListJob *listJob = KIO::listDir(url, KIO::HideProgressInfo);
        connect(listJob, &KIO::ListJob::entries, this, &TestSnapshotWorker::slotGetSubSnapshotsUrl);
        QVERIFY(listJob->exec());
    };

    void testRoot()
    {
        QUrl url;
        url.setScheme("snapshot"_L1);
        url.setHost(m_fsUuid);
        url.setPath("/5"_L1);
        KIO::ListJob *listJob = KIO::listDir(url, KIO::HideProgressInfo);
        connect(listJob, &KIO::ListJob::entries, this, &TestSnapshotWorker::slotRootSnapshotEntries);
        bool ok = listJob->exec();
        if (!ok) {
            qDebug() << url;
            qDebug() << listJob->errorText();
            qDebug() << listJob->errorString();
        }
        QVERIFY(ok);
    }

protected Q_SLOTS:
    void slotAllSubvolumesEntries(KIO::Job *, const KIO::UDSEntryList &entries)
    {
        bool hasRoot = false;
        bool hasSub = false;
        for (const KIO::UDSEntry &entry : std::as_const(entries)) {
            qDebug() << entry;
            const auto displayName = entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
            const auto name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
            if (displayName.contains(QDir::cleanPath(m_testMount + "/sub"_L1))) {
                hasSub = true;
            }
            if (name == "subvolume5"_L1) {
                hasRoot = true;
            }
        }
        QVERIFY(hasSub);
        QVERIFY(hasRoot);
        QCOMPARE(entries.size(), 2);
    }

    void slotGetSubSnapshotsUrl(KIO::Job *, const KIO::UDSEntryList &entries)
    {
        for (const KIO::UDSEntry &entry : std::as_const(entries)) {
            const auto name = entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
            if (name.contains(QDir::cleanPath(m_testMount + "/sub"_L1))) {
                m_subSnapshotsUrl = QUrl(entry.stringValue(KIO::UDSEntry::UDS_URL));
                qDebug() << m_subSnapshotsUrl;
            }
        }

        KIO::ListJob *subListJob = KIO::listDir(m_subSnapshotsUrl, KIO::HideProgressInfo);
        connect(subListJob, &KIO::ListJob::entries, this, &TestSnapshotWorker::slotSubSnapshotEntries);
        QVERIFY(subListJob->exec());
    }

    void slotSubSnapshotEntries(KIO::Job *, const KIO::UDSEntryList &entries)
    {
        QCOMPARE(entries.size(), 6);
    }

    void slotRootSnapshotEntries(KIO::Job *, const KIO::UDSEntryList &entries)
    {
        QCOMPARE(entries.size(), 6);
    }
};

QTEST_GUILESS_MAIN(TestSnapshotWorker)

#include "test_snapshot_worker.moc"
