#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>
# SPDX-License-Identifier: LGPL-2.0-or-later

set -eux

sudo umount butter-tray || true
sudo rmdir butter-tray || true
sudo rm butter || true

truncate --size 128M butter  # 128M is the minimum size for Btrfs, apparently
mkfs.btrfs butter
sudo mount --mkdir --type=btrfs -o uhelper=udisks2 butter butter-tray  # we need udisks to see the mount, so Solid::storageAccessForPath can work
sudo chown -R $USER:$USER butter-tray

sleep 0.5 && sudo btrfs subvolume snapshot butter-tray butter-tray/@initial
echo "hello" > butter-tray/file.txt
sudo btrfs filesystem sync butter-tray
sleep 0.5 && sudo btrfs subvolume snapshot butter-tray butter-tray/@after-creation
sudo btrfs filesystem sync butter-tray
sleep 0.5 && sudo btrfs subvolume snapshot butter-tray butter-tray/@duplicate
echo "world" >> butter-tray/file.txt
sleep 0.5 && sudo btrfs subvolume snapshot butter-tray butter-tray/@after-additions
rm butter-tray/file.txt
sleep 0.5 && sudo btrfs subvolume snapshot butter-tray butter-tray/@after-removal
echo "again" > butter-tray/file.txt
sleep 0.5 && sudo btrfs subvolume snapshot butter-tray butter-tray/@after-recreation

echo "fin" > butter-tray/file.txt  # current

sudo btrfs subvolume create butter-tray/sub
sudo chown -R $USER:$USER butter-tray/sub
sleep 0.5 && sudo btrfs subvolume snapshot butter-tray/sub butter-tray/@sub-initial
echo "hello from subvolume" > butter-tray/sub/vol.txt
sudo btrfs filesystem sync butter-tray
sleep 0.5 && sudo btrfs subvolume snapshot butter-tray/sub butter-tray/@sub-after-creation
sudo btrfs filesystem sync butter-tray
sleep 0.5 && sudo btrfs subvolume snapshot butter-tray/sub butter-tray/@sub-duplicate
echo "world" >> butter-tray/sub/vol.txt
sleep 0.5 && sudo btrfs subvolume snapshot butter-tray/sub butter-tray/@sub-after-additions
rm butter-tray/sub/vol.txt
sleep 0.5 && sudo btrfs subvolume snapshot butter-tray/sub butter-tray/@sub-after-removal
echo "again" > butter-tray/sub/vol.txt
sleep 0.5 && sudo btrfs subvolume snapshot butter-tray/sub butter-tray/@sub-after-recreation

echo "fin" > butter-tray/sub/vol.txt  # current

sudo btrfs filesystem sync butter-tray


# for complex layouts (snapshots and data in separate subvolumes, neither accessible from a single root)

sudo umount butter2-tray || true
sudo rmdir butter2-tray || true
sudo rm butter2 || true

truncate --size 128M butter2  # 128M is the minimum size for Btrfs, apparently
mkfs.btrfs butter2
sudo mount --mkdir --type=btrfs -o uhelper=udisks2 butter2 butter2-tray  # we need udisks to see the mount, so Solid::storageAccessForPath can work
sudo chown -R $USER:$USER butter2-tray

sudo btrfs subvolume create butter2-tray/sub2
sudo btrfs subvolume create butter2-tray/sub2snaps
sudo chown -R $USER:$USER butter2-tray/sub2
sleep 0.5 && sudo btrfs subvolume snapshot butter2-tray/sub2 butter2-tray/sub2snaps/@first
echo "hello from subvolume" > butter2-tray/sub2/data.txt
sleep 0.5 && sudo btrfs subvolume snapshot butter2-tray/sub2 butter2-tray/sub2snaps/@second
echo "fin" > butter2-tray/sub2/data.txt  # current

sudo btrfs filesystem sync butter2-tray

sudo umount butter2-tray
sudo mount --mkdir --type=btrfs -o uhelper=udisks2,subvol=sub2 butter2 butter2-sub2-tray  # we need udisks to see the mount, so Solid::storageAccessForPath can work
sudo mount --mkdir --type=btrfs -o uhelper=udisks2,subvol=sub2snaps butter2 butter2-sub2snaps-tray  # we need udisks to see the mount, so Solid::storageAccessForPath can work

sleep 3  # avoid flakiness with not being able to retrieve mount UUID via DBus
