# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
# SPDX-License-Identifier: LGPL-2.0-or-later

cmake_minimum_required(VERSION 3.16)

project(kio-snapshot VERSION 0.0.1)

set(QT_MIN_VERSION 6.5.0)
set(KF_MIN_VERSION 6.0.0)
set(CMAKE_CXX_STANDARD 20)

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
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    KIO
    I18n
    Solid
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

add_library(btrfssnapshots_lib STATIC common/btrfssnapshots.cpp)
set_target_properties(btrfssnapshots_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(btrfssnapshots_lib Qt::Core PkgConfig::libbtrfsutil)
target_include_directories(btrfssnapshots_lib PUBLIC common/)

add_subdirectory(kioworker)
add_subdirectory(contextmenu)

install(FILES org.kde.kio_snapshot.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
