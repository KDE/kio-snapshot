kcoreaddons_add_plugin(snapshotfileitemaction
    INSTALL_NAMESPACE kf6/kfileitemaction
)

qt_add_dbus_interface(service_dbus_src ${CMAKE_BINARY_DIR}/org.kde.KSnapshotService.xml service_interface)

target_sources(snapshotfileitemaction PRIVATE
    snapshotfileitemaction.cpp
    filesnapshotdialog.cpp
    ${service_dbus_src}
)

ecm_qt_declare_logging_category(snapshotfileitemaction
    HEADER debug.h
    IDENTIFIER SNAPSHOT_FILEITEMACTION
    CATEGORY_NAME org.kde.snapshot.fileitemaction
    DESCRIPTION "Snapshot file item action"
)

target_link_libraries(snapshotfileitemaction KF6::KIOCore KF6::KIOWidgets KF6::I18n KF6::ConfigCore Qt6::DBus)
