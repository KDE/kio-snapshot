# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
# SPDX-License-Identifier: LGPL-2.0-or-later

kcoreaddons_add_plugin(snapshotoverlayicon
    INSTALL_NAMESPACE kf6/overlayicon
)

target_compile_definitions(snapshotoverlayicon PRIVATE
    TRANSLATION_DOMAIN="kio_snapshot"
)

target_sources(snapshotoverlayicon PRIVATE
    snapshotoverlayicon.cpp
    ../kioworker/snapshoturl.cpp
    ${service_dbus_src}
)

ecm_qt_declare_logging_category(snapshotoverlayicon
    HEADER debug.h
    IDENTIFIER SNAPSHOT_OVERLAYICON
    CATEGORY_NAME org.kde.kio_snapshot.overlayicon
    DESCRIPTION "Snapshot overlay icons"
)

target_link_libraries(snapshotoverlayicon KF6::KIOCore KF6::KIOWidgets KF6::I18n KF6::Solid btrfssnapshots_lib)

