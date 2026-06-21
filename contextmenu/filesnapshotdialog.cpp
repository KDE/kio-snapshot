/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "filesnapshotdialog.h"
#include "debug.h"

#include "../common/snapshotinfotypes.h"

#include <KDirModel>
#include <KFileItem>
#include <KFileItemDelegate>
#include <KFormat>
#include <KLocalizedString>

#include <QLayout>
#include <QTreeView>
#include <QUrl>

using namespace Qt::StringLiterals;

class FileSnapshotsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit FileSnapshotsModel(QObject *parent = nullptr)
        : QAbstractTableModel(parent) { };

    void setFiles(const QList<FileSnapshotInfo> &snapshotsInfo)
    {
        beginResetModel();
        QList<FileSnapshotInfo> snapshotsInfoSorted = snapshotsInfo;
        std::sort(snapshotsInfoSorted.begin(), snapshotsInfoSorted.end(), [](const FileSnapshotInfo &a, const FileSnapshotInfo &b) {
            return a.snapshotTimeSecs.value_or(ULONG_LONG_MAX) > b.snapshotTimeSecs.value_or(ULONG_LONG_MAX);
        });
        for (qsizetype i = 0; i < snapshotsInfoSorted.size(); i++) {
            const FileSnapshotInfo &info = snapshotsInfoSorted.at(i);
            if (i == 0 || snapshotsInfoSorted.at(i - 1).generation != info.generation) {
                files << info;
            }
        }
        endResetModel();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : files.count();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 2;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
            switch (section) {
            case 0:
                return i18n("Snapshot date");
            case 1:
                return i18n("Size");
            default:
                return QVariant();
            }
        }
        return QVariant();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() >= files.count())
            return QVariant();

        const FileSnapshotInfo &snapshotInfo = files.at(index.row());
        KFileItem fileItem(QUrl::fromLocalFile(snapshotInfo.path));

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
            case 0:
                if (snapshotInfo.snapshotTimeSecs.has_value()) {
                    QDateTime creation = QDateTime::fromSecsSinceEpoch(snapshotInfo.snapshotTimeSecs.value());
                    return QLocale::system().toString(creation, QLocale::ShortFormat);
                } else {
                    return i18n("Current");
                }
            case 1:
                return KFormat().formatByteSize(fileItem.size());
            default:
                return QVariant();
            }
        }

        if (role == Qt::DecorationRole && index.column() == 0) {
            return QIcon::fromTheme(fileItem.iconName());
        }

        return QVariant();
    }

private:
    QList<FileSnapshotInfo> files;
};

FileSnapshotDialog::FileSnapshotDialog(const QUrl &fileUrl, const QList<FileSnapshotInfo> &snapshotsInfo, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Snapshots for %1", fileUrl.toLocalFile()));

    QLayout *mainLayout = new QVBoxLayout();
    setLayout(mainLayout);

    QTreeView *view = new QTreeView(this);
    FileSnapshotsModel *model = new FileSnapshotsModel();
    view->setModel(model);
    model->setFiles(snapshotsInfo);

    layout()->addWidget(view);
}

#include "filesnapshotdialog.moc"
