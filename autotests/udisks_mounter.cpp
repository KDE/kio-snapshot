/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

// Small util to deal with UDisks for rootless mounting and other tasks in setting up the test Btrfs filesystem

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QStorageInfo>
#include <QString>

#include <fcntl.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

constexpr auto UDISKS2_SERVICE = "org.freedesktop.UDisks2"_L1;

std::optional<QString> setUpLoopDev(const QString &imagePath)
{
    QDBusInterface udisksManager(UDISKS2_SERVICE, "/org/freedesktop/UDisks2/Manager"_L1, "org.freedesktop.UDisks2.Manager"_L1, QDBusConnection::systemBus());

    if (!udisksManager.isValid()) {
        qCritical() << "UDisks2 Manager interface is invalid:" << udisksManager.lastError().message();
        return std::nullopt;
    }

    int imageFd = ::open(imagePath.toUtf8().constData(), O_RDWR);
    auto qImageFd = QDBusUnixFileDescriptor(imageFd);

    QDBusReply<QDBusObjectPath> loopSetupReply = udisksManager.call("LoopSetup"_L1, QVariant::fromValue(qImageFd), QVariantMap());
    if (!loopSetupReply.isValid()) {
        qCritical() << "loop setup failed:" << loopSetupReply.error().message();
        return std::nullopt;
    }

    const QString loopObjectPath = loopSetupReply.value().path();

    return loopObjectPath;
}

std::optional<QString> mountLoopDev(const QString &loopObjectPath, std::optional<QString> subvolume)
{
    QDBusInterface loopObject(UDISKS2_SERVICE, loopObjectPath, "org.freedesktop.UDisks2.Loop"_L1, QDBusConnection::systemBus());
    QDBusInterface loopFsObject(UDISKS2_SERVICE, loopObjectPath, "org.freedesktop.UDisks2.Filesystem"_L1, QDBusConnection::systemBus());

    if (!loopObject.isValid()) {
        qCritical() << "UDisks2 Loop interface is invalid:" << loopObject.lastError().message();
        return std::nullopt;
    }

    if (!loopFsObject.isValid()) {
        qCritical() << "UDisks2 Loop Filesystem interface is invalid:" << loopFsObject.lastError().message();
        return std::nullopt;
    }

    QVariantMap mountOptions;
    if (subvolume.has_value()) {
        mountOptions.insert("options"_L1, "subvol=%1"_L1.arg(*subvolume));
    }
    QDBusReply<QString> loopMountReply = loopFsObject.call("Mount"_L1, mountOptions);
    if (!loopMountReply.isValid()) {
        qCritical() << "loop mount failed:" << loopMountReply.error().message();
        return std::nullopt;
    }

    QDBusReply<void> loopAutoclearReply = loopObject.call("SetAutoclear"_L1, QVariantMap());

    const QString loopMountPath = loopMountReply.value();
    return loopMountPath;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream qOut(stdout);

    QCoreApplication::setApplicationName("udisks_mounter"_L1);
    QCoreApplication::setApplicationVersion("0.1"_L1);

    QCommandLineParser argparse;
    argparse.setApplicationDescription("Mounts a disk image using udisks"_L1);
    argparse.addHelpOption();
    argparse.addOption(QCommandLineOption("mount-image"_L1, "Mount a disk image"_L1, "image"_L1));
    argparse.addOption(QCommandLineOption("subvolume"_L1, "Subvolume inside the FS to mount"_L1, "subvolume"_L1));
    argparse.addOption(QCommandLineOption("unmount-path"_L1, "Unmount a mountpoint"_L1, "path"_L1));

    argparse.process(app);

    if (!QDBusConnection::systemBus().isConnected()) {
        qCritical() << "Cannot connect to the DBus system bus:" << QDBusConnection::systemBus().lastError().message();
        return 1;
    }

    if (argparse.isSet("mount-image"_L1)) {
        const QString imagePath = argparse.value("mount-image"_L1);

        const auto loopObjectPath = setUpLoopDev(imagePath);
        if (!loopObjectPath.has_value()) {
            return 1;
        }

        const auto loopMountPath =
            mountLoopDev(*loopObjectPath, argparse.isSet("subvolume"_L1) ? std::make_optional(argparse.value("subvolume"_L1)) : std::nullopt);
        if (!loopMountPath.has_value()) {
            return 1;
        }

        qOut << *loopMountPath << Qt::endl;
    } else if (argparse.isSet("unmount-path"_L1)) {
        const QString mountPath = argparse.value("unmount-path"_L1);
        QStorageInfo storage(mountPath);
        const QString device = QString::fromUtf8(storage.device());
        const QString deviceBasename = device.mid("/dev/"_L1.length());

        QDBusInterface deviceObject(UDISKS2_SERVICE,
                                    "/org/freedesktop/UDisks2/block_devices/%1"_L1.arg(deviceBasename),
                                    "org.freedesktop.UDisks2.Filesystem"_L1,
                                    QDBusConnection::systemBus());

        if (!deviceObject.isValid()) {
            qCritical() << "UDisks2 Filesystem interface is invalid:" << deviceObject.lastError().message() << "for" << device;
        }

        deviceObject.call("Unmount"_L1, QVariantMap());
    }

    return 0;
}
