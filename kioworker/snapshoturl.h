/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QUrl>

enum class SnapshotUrlKind {
    Subvolume,
    File,
};

class SnapshotUrl : public QUrl
{
public:
    SnapshotUrl(const QUrl &url);

    QString fsRoot() const;
    std::optional<SnapshotUrlKind> kind() const;
    std::optional<qulonglong> subvolumeId() const;
    std::optional<qulonglong> snapshotId() const;
    QString actualPath() const;
};
