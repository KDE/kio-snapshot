kcoreaddons_add_plugin(kio_snapshot
    INSTALL_NAMESPACE kf6/kio
)

set_source_files_properties(${CMAKE_SOURCE_DIR}/service/org.kde.KSnapshotService.xml
PROPERTIES INCLUDE ${CMAKE_SOURCE_DIR}/common/snapshotinfotypes.h)

qt_add_dbus_interface(service_dbus_src ${CMAKE_SOURCE_DIR}/service/org.kde.KSnapshotService.xml service_interface)

target_sources(kio_snapshot PRIVATE
    snapshot.cpp
    ${service_dbus_src}
)

ecm_qt_declare_logging_category(kio_snapshot
    HEADER debug.h
    IDENTIFIER KIO_SNAPSHOT
    CATEGORY_NAME kf.kio.workers.snapshot
    DESCRIPTION "kio_snapshot (KIO)"
    EXPORT KIO
)

target_link_libraries(kio_snapshot KF6::KIOCore KF6::I18n KF6::ConfigCore Qt6::DBus)
