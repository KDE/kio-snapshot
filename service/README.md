# KDE Snapshot Service

A DBus service to provide high-level access to common operations with (Btrfs) snapshots as required in KDE software.

Btrfs requires root access (`CAP_SYS_ADMIN`) to access any metadata about subvolumes and their relationships.
So, this service provides access to that metadata to regular users, but only for directories they own. In that way we preserve the overall privacy of the filesystem layout.

## API

### `getSubvolumeForPath`

Input: Path

Output: Subvolume ID

### `getPathForSubvolume`

Input: Subvolume ID

Output: Path

### `getSnapshotsForSubvolume`

Input: Subvolume ID

Output: Information about the snapshot subvolumes of this subvolume.

Output format is an array of variants, with the following keys:
  - `SubvolumeId`
  - `Path`
  - `CreationTimeSec` & `CreationTimeNanosec` -- when the snapshot was created

### `getSnapshotsForFile`

Input: Path to a file

Output: Information about the prior versions of this file, as found in various snapshots of its containing subvolume if they exist.

Output format is an array of variants, where all except the last one have the following keys:

  - `Path`
  - `SubvolumeId`
  - `SnapshotCreationTimeSec` & `SnapshotCreationTimeNanosec` -- when the containing snapshot subvolume was created
  - `ModificationTimeSec` & `ModificationTimeNanosec` -- the file's own modification time
    
The last in the array represents the file that was queried itself, and contains all the keys above except for `SnapshotCreationTimeSec` and `SnapshotCreationTimeNanosec`.

