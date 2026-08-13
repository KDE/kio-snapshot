# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
# SPDX-License-Identifier: LGPL-2.0-or-later

kcoreaddons_add_plugin(kio_snapshot
    INSTALL_NAMESPACE kf6/kio
)

target_compile_definitions(kio_snapshot PRIVATE
    TRANSLATION_DOMAIN="kio_snapshot"
)

target_sources(kio_snapshot PRIVATE
    snapshot.cpp
    filesnapshots.cpp
    snapshoturl.cpp
    subvolumesnapshots.cpp
)

ecm_qt_declare_logging_category(kio_snapshot
    HEADER snapshot_debug.h
    IDENTIFIER KIO_SNAPSHOT
    CATEGORY_NAME kf.kio.workers.snapshot
    DESCRIPTION "kio_snapshot (KIO)"
    EXPORT KIO
)

target_link_libraries(kio_snapshot KF6::KIOCore KF6::I18n KF6::Solid btrfssnapshots_lib)
