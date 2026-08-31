# Testing kio-snapshot

The testing relies on a Btrfs filesystem with snapshots.
The included `prep_btrfs.sh` creates two empty 128 MB Btrfs filesystem images:
  - one is mounted at `butter-tray` in the current working directory,
  - the other has two subvolumes that are mounted separately at `butter2-sub2-tray` and `butter2-sub2snaps-tray` in the current working directory.

Please pass the path to the `butter-tray` mountpoint as the environment variable `KIO_SNAPSHOT_TEST_MOUNTPOINT`, and to the `butter2-sub2-tray` mountpoint as `KIO_SNAPSHOT_TEST_COMPLEX_MOUNTPOINT`.
