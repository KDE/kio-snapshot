/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "snapshotfileitemaction.h"
#include "debug.h"

#include "service_interface.h"

#include <KFileItem>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>
#include <QIcon>
#include <QList>
#include <QMenu>
#include <QUrl>

using namespace Qt::StringLiterals;

K_PLUGIN_CLASS_WITH_JSON(SnapshotFileItemAction, "snapshotfileitemaction.json")

SnapshotFileItemAction::SnapshotFileItemAction(QObject *parent)
    : KAbstractFileItemActionPlugin(parent)
    , service(new org::kde::ksnapshotservice("org.kde.ksnapshotservice"_L1, "/KSnapshotService"_L1, QDBusConnection::systemBus(), this))
{
}

QList<QAction *> SnapshotFileItemAction::actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget)
{
    QList<QAction *> actions;

    if (fileItemInfos.urlList().length() != 1) {
        return actions;
    }

    QUrl itemUrl = fileItemInfos.urlList().first();
    KFileItem item = fileItemInfos.items().findByUrl(itemUrl);
    if (itemUrl.scheme() == "snapshot"_L1 || !item.isLocalFile()) {
        // TODO support for directories
        return actions;
    }

    auto snapshotQueryReply = service->getSnapshotsForFile(itemUrl.toLocalFile());
    snapshotQueryReply.waitForFinished();
    if (!snapshotQueryReply.isValid()) {
        return actions;
    }
    QVariantList snapshotsVars = snapshotQueryReply.value();
    QList<QVariantMap> snapshots;
    for (const QVariant &snapshotV : snapshotsVars) {
        snapshots << qdbus_cast<QVariantMap>(snapshotV.value<QDBusArgument>());
    }

    QAction *action = new QAction(QIcon::fromTheme("view-history"_L1), i18n("View snapshots…"), parentWidget);
    // TODO actually implement the dialog box...
    actions << action;

    return actions;
}

#include "snapshotfileitemaction.moc"
