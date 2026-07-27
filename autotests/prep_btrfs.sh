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

sudo btrfs subvolume snapshot butter-tray butter-tray/@initial
echo "hello" > butter-tray/file.txt
sudo btrfs subvolume snapshot butter-tray butter-tray/@after-creation
sudo btrfs subvolume snapshot butter-tray butter-tray/@duplicate
echo "world" >> butter-tray/file.txt
sudo btrfs subvolume snapshot butter-tray butter-tray/@after-additions
rm butter-tray/file.txt
sudo btrfs subvolume snapshot butter-tray butter-tray/@after-removal
echo "again" > butter-tray/file.txt
sudo btrfs subvolume snapshot butter-tray butter-tray/@after-recreation

sudo btrfs subvolume create butter-tray/sub
sudo chown -R $USER:$USER butter-tray/sub
sudo btrfs subvolume snapshot butter-tray/sub butter-tray/@sub-initial
echo "hello from subvolume" > butter-tray/sub/vol.txt
sudo btrfs subvolume snapshot butter-tray/sub butter-tray/@sub-after-creation
sudo btrfs subvolume snapshot butter-tray/sub butter-tray/@sub-duplicate
echo "world" >> butter-tray/sub/vol.txt
sudo btrfs subvolume snapshot butter-tray/sub butter-tray/@sub-after-additions
rm butter-tray/sub/vol.txt
sudo btrfs subvolume snapshot butter-tray/sub butter-tray/@sub-after-removal
echo "again" > butter-tray/sub/vol.txt
sudo btrfs subvolume snapshot butter-tray/sub butter-tray/@sub-after-recreation
