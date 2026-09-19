#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
# SPDX-License-Identifier: LGPL-2.0-or-later

set -eux

sudo pacman --sync --refresh --noconfirm btrfs-progs kio solid ki18n cmake base-devel extra-cmake-modules ninja bubblewrap
mkdir build
cmake -B build -S . -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
export KIO_SNAPSHOT_BUILD_DIR="$PWD/build"
mkdir fstmp
cd fstmp
bash ../autotests/prep_btrfs.sh
cd ..
export KIO_SNAPSHOT_TEST_MOUNTPOINT="$PWD/fstmp/butter-tray"
export KIO_SNAPSHOT_TEST_COMPLEX_MOUNTPOINT="$PWD/fstmp/butter2-sub2-tray"
export QT_LOGGING_RULES="default.debug=true;*snapshot*=true"
bwrap \
    --bind / / \
    --bind "$PWD/fstmp/butter2-tray/sub2" "$PWD/fstmp/butter2-sub2-tray" \
    --bind "$PWD/fstmp/butter2-tray/sub2snaps" "$PWD/fstmp/butter2-sub2snaps-tray" \
    --ro-bind /dev/null "$PWD/fstmp/butter2-tray" \
    cmake --build build --target test

