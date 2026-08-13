/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "snapshoturl.h"

#include <Solid/Device>
#include <Solid/StorageAccess>

#include <QString>
#include <QUuid>

using namespace Qt::StringLiterals;

SnapshotUrl::SnapshotUrl(const QUrl &url)
    : QUrl(url)
{
}

QString SnapshotUrl::fsRoot() const
{
    if (host().isEmpty()) {
        return "/"_L1;
    }
    QUuid uuid(host());
    if (!uuid.isNull()) {
        const auto deviceList = Solid::Device::listFromQuery("StorageVolume.uuid == '%1'"_L1.arg(uuid.toString(QUuid::StringFormat::WithoutBraces).toLower()));
        if (!deviceList.isEmpty()) {
            auto device = deviceList.first();
            auto storageAccess = device.as<Solid::StorageAccess>();
            if (storageAccess) {
                return storageAccess->filePath();
            }
        }
    }
    return "/"_L1;
}

std::optional<SnapshotUrlKind> SnapshotUrl::kind() const
{
    const QString firstSegment = path().section('/'_L1, 0, 0, QString::SectionFlag::SectionSkipEmpty);
    if (firstSegment == "file"_L1) {
        return SnapshotUrlKind::File;
    } else if (firstSegment == "subvolume"_L1) {
        return SnapshotUrlKind::Subvolume;
    }
    return std::nullopt;
}

std::optional<qulonglong> SnapshotUrl::subvolumeId() const
{
    if (kind() == SnapshotUrlKind::Subvolume) {
        bool ok;
        qulonglong id = path().section('/'_L1, 1, 1, QString::SectionFlag::SectionSkipEmpty).toULongLong(&ok);
        if (!ok) {
            return std::nullopt;
        }
        return id;
    }
    return std::nullopt;
}

std::optional<qulonglong> SnapshotUrl::snapshotId() const
{
    if (kind() == SnapshotUrlKind::Subvolume) {
        bool ok;
        qulonglong id = path().section('/'_L1, 2, 2, QString::SectionFlag::SectionSkipEmpty).toULongLong(&ok);
        if (!ok) {
            return std::nullopt;
        }
        return id;
    }
    return std::nullopt;
}

QString SnapshotUrl::actualPath() const
{
    return '/'_L1 + path().section('/'_L1, (kind() == SnapshotUrlKind::Subvolume ? 3 : 1), -1, QString::SectionFlag::SectionSkipEmpty);
}
