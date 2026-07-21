# KIO Snapshot

Integration of Btrfs filesystem snapshots for KDE apps.

Includes:

  - [KIO workers](./kioworker) for browsing snapshots of a subvolume, and for browsing previous versions of a file
  - a [KFileItemActions plugin (context menu)](./contextmenu) for exposing these as menu items in Dolphin etc.

## A note for users of Snapper

If you use [Snapper](http://snapper.io/) as your tool for managing snapshots, you will need to add yourself to the `ALLOW_USERS` setting for your Snapper config, and also turn on `SYNC_ACL=yes`.
See http://snapper.io/manpages/snapper-configs.html and http://snapper.io/manpages/snapper-configs.html for more info.

Distro integrators, if you want to ship KIO Snapshot and Snapper for snapshots of the user's home, please make sure that the above is already configured.
