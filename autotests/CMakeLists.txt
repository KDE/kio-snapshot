# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
# SPDX-License-Identifier: LGPL-2.0-or-later

add_executable(SnapshotWorkerTest test_snapshot_worker.cpp)
target_link_libraries(SnapshotWorkerTest PRIVATE Qt6::Core Qt6::Test KF6::KIOCore KF6::Solid)
add_test(NAME SnapshotWorkerTest COMMAND SnapshotWorkerTest)

add_executable(FileSnapshotsWorkerTest test_filesnapshots_worker.cpp)
target_link_libraries(FileSnapshotsWorkerTest PRIVATE Qt6::Core Qt6::Test KF6::KIOCore)
add_test(NAME FileSnapshotsWorkerTest COMMAND FileSnapshotsWorkerTest)
