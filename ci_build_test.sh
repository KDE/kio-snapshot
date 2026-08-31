#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
# SPDX-License-Identifier: LGPL-2.0-or-later

set -eux

sudo pacman --sync --refresh --noconfirm btrfs-progs kio solid ki18n cmake base-devel extra-cmake-modules ninja
mkdir fstmp
cd fstmp
bash ../autotests/prep_btrfs.sh
cd ..
mkdir build
cmake -B build -S . -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
export KIO_SNAPSHOT_TEST_MOUNTPOINT="$PWD/fstmp/butter-tray"
export KIO_SNAPSHOT_TEST_COMPLEX_MOUNTPOINT="$PWD/fstmp/butter2-sub2-tray"
export QT_LOGGING_RULES="default.debug=true;*snapshot*=true"
cmake --build build --target test
