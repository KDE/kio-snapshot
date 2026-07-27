# Testing kio-snapshot

The testing relies on a Btrfs filesystem with snapshots.
The included `prep_btrfs.sh` creates an empty 128 MB Btrfs filesystem image,
mounts it at `butter-tray` in the working directory, and sets up test files
and subvolumes and snapshots inside it.

Please pass the path to the mountpoint as the environment variable `KIO_SNAPSHOT_TEST_MOUNTPOINT`.
