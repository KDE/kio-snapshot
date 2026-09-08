/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "debug.h"

#include "../kioworker/snapshoturl.h"
#include <btrfssnapshots.h>

#include <Solid/Device>
#include <Solid/StorageAccess>
#include <Solid/StorageVolume>

#include <KFileItem>
#include <KLocalizedString>
#include <KOverlayIconPlugin>
#include <KPluginFactory>

#include <QAction>
#include <QDBusMetaType>
#include <QDir>
#include <QIcon>
#include <QList>
#include <QMenu>
#include <QUrl>

using namespace Qt::StringLiterals;

class SnapshotOverlayIconPlugin : public KOverlayIconPlugin
{
    Q_PLUGIN_METADATA(IID "org.kde.kio_snapshot.overlayicon" FILE "snapshotoverlayicon.json")
    Q_OBJECT

public:
    SnapshotOverlayIconPlugin()
    {
    }

    QStringList getOverlays(const QUrl &url) override
    {
        if (url.scheme() != "snapshot"_L1) {
            return {};
        }
        const SnapshotUrl snapshotUrl(url);
        if (snapshotUrl.kind() == SnapshotUrlKind::File) {
            return {"view-history"_L1};
        } else {
            if (snapshotUrl.subvolumeId().has_value() && snapshotUrl.snapshotId().has_value() && snapshotUrl.actualPath().length() > 1) {
                return {"view-history"_L1};
            }
        }
        return {};
    }
};

#include "snapshotoverlayicon.moc"
