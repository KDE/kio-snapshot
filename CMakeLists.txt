cmake_minimum_required(VERSION 3.16)

project(kde-snapshot-service VERSION 0.0.1)

set(QT_MIN_VERSION 6.5.0)
set(KF_MIN_VERSION 6.0.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

include(ECMSetupVersion)
include(ECMDeprecationSettings)
include(ECMQtDeclareLoggingCategory)
include(KDEClangFormat)
include(KDEGitCommitHooks)

include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    DBus
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    KIO
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libbtrfsutil REQUIRED IMPORTED_TARGET libbtrfsutil)

ecm_set_disabled_deprecation_versions(
    QT 6.7
    KF 6.4
)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})

kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

ecm_qt_declare_logging_category(GENERATED_SOURCES
    HEADER "ksnapshotservice_debug.h"
    IDENTIFIER "KSNAPSHOTSERVICE_LOG"
    CATEGORY_NAME "org.kde.ksnapshotservice"
    DEFAULT_SEVERITY Debug
)

qt_generate_dbus_interface(ksnapshotservice.h org.kde.KSnapshotService.xml OPTIONS -a)

add_executable(ksnapshotservice
    ksnapshotservice.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ksnapshotservice_debug.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KSnapshotService.xml
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
  ${CMAKE_CURRENT_BINARY_DIR}/org.kde.KSnapshotService.xml
  DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})

