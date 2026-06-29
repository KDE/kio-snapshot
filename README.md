# KDE Snapshot Integration

User-facing integrations for Btrfs snapshots, and the [DBus service](./service) supporting those.

Includes:

  - a [KIO worker](./kioworker) for browsing snapshots of a subvolume, and one for browsing previous versions of a file
  - a [KFileItemActions plugin (context menu)](./contextmenu) for exposing these as menu items in Dolphin etc.
