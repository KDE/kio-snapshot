#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
# SPDX-License-Identifier: LGPL-2.0-or-later

set -eux

rm butter || true

truncate --size 128M butter  # 128M is the minimum size for Btrfs, apparently
mkdir __test  # something for mkfs to copy permissions from, so we can read-write to it later
mkfs.btrfs --rootdir __test butter
rmdir __test

echo $KIO_SNAPSHOT_BUILD_DIR
if ! [ -n "$KIO_SNAPSHOT_BUILD_DIR" ]; then
    echo "KIO_SNAPSHOT_BUILD_DIR not set (required for udisks_mounter)"
    exit 1
fi

PATH="$KIO_SNAPSHOT_BUILD_DIR/bin/:$PATH"

export QT_LOGGING_RULES="*.critical=true;default.debug=true"

BUTTER_TRAY=$(udisks_mounter --mount-image butter)

sleep 0.5 && btrfs subvolume snapshot "$BUTTER_TRAY" "$BUTTER_TRAY/@initial"
echo "hello" > "$BUTTER_TRAY/file.txt"
btrfs filesystem sync "$BUTTER_TRAY"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER_TRAY" "$BUTTER_TRAY/@after-creation"
btrfs filesystem sync "$BUTTER_TRAY"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER_TRAY" "$BUTTER_TRAY/@duplicate"
echo "world" >> "$BUTTER_TRAY/file.txt"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER_TRAY" "$BUTTER_TRAY/@after-additions"
rm "$BUTTER_TRAY/file.txt"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER_TRAY" "$BUTTER_TRAY/@after-removal"
echo "again" > "$BUTTER_TRAY/file.txt"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER_TRAY" "$BUTTER_TRAY/@after-recreation"

echo "fin" > "$BUTTER_TRAY/file.txt"  # current

btrfs subvolume create "$BUTTER_TRAY/sub"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER_TRAY/sub" "$BUTTER_TRAY/@sub-initial"
echo "hello from subvolume" > "$BUTTER_TRAY/sub/vol.txt"
btrfs filesystem sync "$BUTTER_TRAY"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER_TRAY/sub" "$BUTTER_TRAY/@sub-after-creation"
btrfs filesystem sync "$BUTTER_TRAY"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER_TRAY/sub" "$BUTTER_TRAY/@sub-duplicate"
echo "world" >> "$BUTTER_TRAY/sub/vol.txt"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER_TRAY/sub" "$BUTTER_TRAY/@sub-after-additions"
rm "$BUTTER_TRAY/sub/vol.txt"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER_TRAY/sub" "$BUTTER_TRAY/@sub-after-removal"
echo "again" > "$BUTTER_TRAY/sub/vol.txt"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER_TRAY/sub" "$BUTTER_TRAY/@sub-after-recreation"

echo "fin" > "$BUTTER_TRAY/sub/vol.txt"  # current

btrfs filesystem sync "$BUTTER_TRAY"

ln -sf "$BUTTER_TRAY" butter-tray


# for complex layouts (snapshots and data in separate subvolumes, neither accessible from a single root)

rm butter2 || true

truncate --size 128M butter2  # 128M is the minimum size for Btrfs, apparently
mkdir __test  # something for mkfs to copy permissions from, so we can read-write to it later
mkfs.btrfs --rootdir __test butter2
rmdir __test

BUTTER2_TRAY=$(udisks_mounter --mount-image butter2)

btrfs subvolume create "$BUTTER2_TRAY/sub2"
btrfs subvolume create "$BUTTER2_TRAY/sub2snaps"
chown -R $USER:$USER "$BUTTER2_TRAY/sub2"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER2_TRAY/sub2" "$BUTTER2_TRAY/sub2snaps/@first"
echo "hello from subvolume" > "$BUTTER2_TRAY/sub2/data.txt"
sleep 0.5 && btrfs subvolume snapshot "$BUTTER2_TRAY/sub2" "$BUTTER2_TRAY/sub2snaps/@second"
echo "fin" > "$BUTTER2_TRAY/sub2/data.txt"  # current

btrfs filesystem sync "$BUTTER2_TRAY"

ln -sf "$BUTTER2_TRAY" butter2-tray

sleep 3  # avoid flakiness with not being able to retrieve mount UUID via DBus
