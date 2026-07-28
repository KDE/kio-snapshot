#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
# SPDX-License-Identifier: LGPL-2.0-or-later

set -eux

sudo pacman --sync --refresh --noconfirm btrfs-progs kio solid ki18n cmake base-devel extra-cmake-modules ninja
mkdir build
cmake -B build -S . -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
SOURCEDIR=$PWD
cd /tmp
bash "$SOURCEDIR/autotests/prep_btrfs.sh"
cd "$SOURCEDIR"
export KIO_SNAPSHOT_TEST_MOUNTPOINT=/tmp/butter-tray
export QT_LOGGING_RULES="default.debug=true"
cmake --build build --target test
