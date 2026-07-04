/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <KAbstractFileItemActionPlugin>
#include <KFileItemListProperties>

class QAction;

class SnapshotFileItemAction : public KAbstractFileItemActionPlugin
{
    Q_OBJECT
public:
    SnapshotFileItemAction(QObject *parent);

    QList<QAction *> actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget) override;
};

#endif
