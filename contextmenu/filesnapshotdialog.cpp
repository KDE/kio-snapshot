/*
    SPDX-FileCopyrightText: 2026 Bharadwaj Raju <bharadwaj.raju@machinesoul.in>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "filesnapshotdialog.h"
#include "debug.h"

#include "../common/snapshotinfotypes.h"

#include <KIO/CopyJob>
#include <KIO/OpenUrlJob>

#include <KDirModel>
#include <KFileItem>
#include <KFileItemDelegate>
#include <KFormat>
#include <KLocalizedString>

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLayout>
#include <QPushButton>
#include <QTreeView>
#include <QUrl>

using namespace Qt::StringLiterals;

const int PathRole = Qt::UserRole + 1;

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
            if (i == 0 || snapshotsInfoSorted.at(i - 1).modificationTimeSecs != info.modificationTimeSecs
                || snapshotsInfoSorted.at(i - 1).modificationTimeNanosecs != info.modificationTimeNanosecs) {
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
                    return fmt.formatRelativeDateTime(creation, QLocale::LongFormat);
                } else {
                    return i18n("Current");
                }
            case 1:
                return fmt.formatByteSize(fileItem.size());
            default:
                return QVariant();
            }
        }

        if (role == Qt::DecorationRole && index.column() == 0) {
            return QIcon::fromTheme(fileItem.iconName());
        }

        if (role == PathRole) {
            return snapshotInfo.path;
        }

        return QVariant();
    }

private:
    QList<FileSnapshotInfo> files;
    KFormat fmt;
};

FileSnapshotDialog::FileSnapshotDialog(const QUrl &fileUrl, const QList<FileSnapshotInfo> &snapshotsInfo, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Snapshots for %1", fileUrl.toLocalFile()));
    resize(400, 500);

    QVBoxLayout *mainLayout = new QVBoxLayout();
    setLayout(mainLayout);

    QTreeView *view = new QTreeView(this);
    view->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
    view->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
    view->setRootIsDecorated(false);
    view->setItemsExpandable(false);
    view->setAlternatingRowColors(true);
    FileSnapshotsModel *model = new FileSnapshotsModel();
    view->setModel(model);
    model->setFiles(snapshotsInfo);
    view->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    connect(view, &QTreeView::doubleClicked, this, [this, model](const QModelIndex &idx) {
        if (idx.isValid()) {
            QString path = model->data(idx, PathRole).toString();
            KJob *openUrlJob = new KIO::OpenUrlJob(QUrl::fromLocalFile(path), this);
            openUrlJob->start();
        }
    });

    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    QPushButton *openBtn = new QPushButton(i18n("Open"), this);
    openBtn->setIcon(QIcon::fromTheme("document-open"_L1));
    connect(openBtn, &QPushButton::clicked, this, [this, view, model]() {
        QModelIndex idx = view->selectionModel()->selectedRows().first();
        QString path = model->data(idx, PathRole).toString();
        KJob *openUrlJob = new KIO::OpenUrlJob(QUrl::fromLocalFile(path), this);
        openUrlJob->start();
    });
    openBtn->setEnabled(false);
    QPushButton *restoreBtn = new QPushButton(i18n("Restore"), this);
    restoreBtn->setIcon(QIcon::fromTheme("document-revert"_L1));
    connect(restoreBtn, &QPushButton::clicked, this, [view, model, fileUrl]() {
        QModelIndex idx = view->selectionModel()->selectedRows().first();
        QString srcPath = model->data(idx, PathRole).toString();
        KJob *copyJob = KIO::copy(QUrl::fromLocalFile(srcPath), fileUrl, KIO::Overwrite);
        copyJob->start();
    });
    restoreBtn->setEnabled(false);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(openBtn);
    buttonsLayout->addWidget(restoreBtn);

    connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [view, openBtn, restoreBtn]() {
        openBtn->setEnabled(view->selectionModel()->hasSelection());
        restoreBtn->setEnabled(view->selectionModel()->hasSelection() && view->selectionModel()->selectedRows().first().row() != 0);
    });

    layout()->addWidget(view);
    layout()->addItem(buttonsLayout);
}

#include "filesnapshotdialog.moc"
