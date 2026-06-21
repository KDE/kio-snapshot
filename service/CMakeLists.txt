ecm_qt_declare_logging_category(GENERATED_SOURCES
    HEADER "ksnapshotservice_debug.h"
    IDENTIFIER "KSNAPSHOTSERVICE_LOG"
    CATEGORY_NAME "org.kde.ksnapshotservice"
    DEFAULT_SEVERITY Debug
)

add_executable(ksnapshotservice
    ksnapshotservice.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ksnapshotservice_debug.cpp
)

target_link_libraries(ksnapshotservice
    Qt6::Core
    Qt6::DBus
    PkgConfig::libbtrfsutil
)

configure_file(org.kde.ksnapshotservice.service.in org.kde.ksnapshotservice.service)
configure_file(ksnapshotservice.service.in ksnapshotservice.service)

install(TARGETS
  ksnapshotservice
  DESTINATION ${KDE_INSTALL_LIBEXECDIR})
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/org.kde.ksnapshotservice.service
  DESTINATION ${KDE_INSTALL_DBUSSYSTEMSERVICEDIR})
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ksnapshotservice.service
  DESTINATION ${KDE_INSTALL_SYSTEMDUNITDIR}/system)
install(FILES
  org.kde.ksnapshotservice.conf
  DESTINATION ${KDE_INSTALL_DBUSDIR}/system.d/)
install(FILES
  ${CMAKE_BINARY_DIR}/org.kde.KSnapshotService.xml
  DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})
