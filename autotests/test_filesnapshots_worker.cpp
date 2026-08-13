/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/ListJob>

#include <QDir>
#include <QObject>
#include <QString>
#include <QtTest>

using namespace Qt::StringLiterals;

class TestFileSnapshotsWorker : public QObject
{
    Q_OBJECT

private:
    QString m_testMount;

private Q_SLOTS:
    void initTestCase()
    {
        m_testMount = QString::fromUtf8(qgetenv("KIO_SNAPSHOT_TEST_MOUNTPOINT"));
        if (m_testMount.isEmpty() || !QDir(m_testMount).exists()) {
            QFAIL("Could not access mountpoint for test Btrfs filesystem (check env var KIO_SNAPSHOT_TEST_MOUNTPOINT)");
        }
    };

    void testRootFile()
    {
        QUrl url;
        url.setScheme("snapshot"_L1);
        url.setPath(QDir::cleanPath("/file/"_L1 + m_testMount + "/file.txt"_L1));
        KIO::ListJob *listJob = KIO::listDir(url, KIO::HideProgressInfo);
        connect(listJob, &KIO::ListJob::entries, this, &TestFileSnapshotsWorker::slotRootFileEntries);
        QVERIFY(listJob->exec());
    };

    void testSubvolumeFile()
    {
        QUrl url;
        url.setScheme("snapshot"_L1);
        url.setPath(QDir::cleanPath("/file/"_L1 + m_testMount + "/sub/vol.txt"_L1));
        KIO::ListJob *listJob = KIO::listDir(url, KIO::HideProgressInfo);
        connect(listJob, &KIO::ListJob::entries, this, &TestFileSnapshotsWorker::slotSubvolumeFileEntries);
        QVERIFY(listJob->exec());
    };

protected Q_SLOTS:
    void slotRootFileEntries(KIO::Job *, const KIO::UDSEntryList &entries)
    {
        bool hasCurrent = false;
        bool hasAfterCreation = false;
        bool hasDuplicate = false;
        bool hasAfterAdditions = false;
        bool hasAfterRemoval = false;
        bool hasAfterRecreation = false;
        for (const KIO::UDSEntry &entry : std::as_const(entries)) {
            qDebug() << entry;
            const auto localPath = entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);
            QVERIFY(!localPath.isEmpty());
            if (localPath == QDir::cleanPath(m_testMount + "/file.txt"_L1)) {
                hasCurrent = true;
            }
            if (localPath.contains("@after-creation"_L1)) {
                hasAfterCreation = true;
            }
            if (localPath.contains("@duplicate"_L1)) {
                hasDuplicate = true;
            }
            if (localPath.contains("@after-additions"_L1)) {
                hasAfterAdditions = true;
            }
            if (localPath.contains("@after-removal"_L1)) {
                hasAfterRemoval = true;
            }
            if (localPath.contains("@after-recreation"_L1)) {
                hasAfterRecreation = true;
            }
        }
        QVERIFY(hasCurrent);
        QVERIFY(hasAfterCreation);
        QVERIFY(!hasDuplicate);
        QVERIFY(hasAfterAdditions);
        QVERIFY(!hasAfterRemoval);
        QVERIFY(hasAfterRecreation);
    }

    void slotSubvolumeFileEntries(KIO::Job *, const KIO::UDSEntryList &entries)
    {
        bool hasCurrent = false;
        bool hasAfterCreation = false;
        bool hasDuplicate = false;
        bool hasAfterAdditions = false;
        bool hasAfterRemoval = false;
        bool hasAfterRecreation = false;
        for (const KIO::UDSEntry &entry : std::as_const(entries)) {
            qDebug() << entry;
            const auto localPath = entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);
            QVERIFY(!localPath.isEmpty());
            if (localPath == QDir::cleanPath(m_testMount + "/sub/vol.txt"_L1)) {
                hasCurrent = true;
            }
            if (localPath.contains("@sub-after-creation"_L1)) {
                hasAfterCreation = true;
            }
            if (localPath.contains("@sub-duplicate"_L1)) {
                hasDuplicate = true;
            }
            if (localPath.contains("@sub-after-additions"_L1)) {
                hasAfterAdditions = true;
            }
            if (localPath.contains("@sub-after-removal"_L1)) {
                hasAfterRemoval = true;
            }
            if (localPath.contains("@sub-after-recreation"_L1)) {
                hasAfterRecreation = true;
            }
        }
        QVERIFY(hasCurrent);
        QVERIFY(hasAfterCreation);
        QVERIFY(!hasDuplicate);
        QVERIFY(hasAfterAdditions);
        QVERIFY(!hasAfterRemoval);
        QVERIFY(hasAfterRecreation);
    }
};

QTEST_GUILESS_MAIN(TestFileSnapshotsWorker)

#include "test_filesnapshots_worker.moc"
