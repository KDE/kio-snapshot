/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef FILESNAPSHOTDIALOG_H
#define FILESNAPSHOTDIALOG_H

#include <QDialog>

class FileSnapshotInfo;

class FileSnapshotDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FileSnapshotDialog(const QUrl &fileUrl, const QList<FileSnapshotInfo> &snapshotInfo, QWidget *parent = nullptr);
};

#endif
