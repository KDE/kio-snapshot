# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
# SPDX-License-Identifier: LGPL-2.0-or-later

add_executable(UrlParsingTest test_url_parsing.cpp ../kioworker/snapshoturl.cpp)
target_link_libraries(UrlParsingTest PRIVATE Qt6::Core Qt6::Test KF6::KIOCore KF6::Solid)
add_test(NAME UrlParsingTest COMMAND UrlParsingTest -o url-parsing-report.xml,junitxml)

add_executable(SnapshotWorkerTest test_snapshot_worker.cpp)
target_link_libraries(SnapshotWorkerTest PRIVATE Qt6::Core Qt6::Test KF6::KIOCore KF6::Solid)
add_test(NAME SnapshotWorkerTest COMMAND SnapshotWorkerTest -o snapshot-worker-report.xml,junitxml)

add_executable(FileSnapshotsWorkerTest test_filesnapshots_worker.cpp)
target_link_libraries(FileSnapshotsWorkerTest PRIVATE Qt6::Core Qt6::Test KF6::KIOCore)
add_test(NAME FileSnapshotsWorkerTest COMMAND FileSnapshotsWorkerTest -o filesnapshots-worker-report.xml,junitxml)

add_executable(BtrfsSnapshotsCoreTest test_btrfssnapshots_core.cpp)
target_link_libraries(BtrfsSnapshotsCoreTest PRIVATE Qt6::Core Qt6::Test btrfssnapshots_lib)
add_test(NAME BtrfsSnapshotsCoreTest COMMAND BtrfsSnapshotsCoreTest -o btrfssnapshots-core-report.xml,junitxml)
