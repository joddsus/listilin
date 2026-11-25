#pragma once

#include <QAbstractListModel>
#include <QThread>
#include <QVector>

#include "entry.h"
#include "fileops.h"

class DirWorker;

// Holds one listing and the sort state for it. Scanning happens in a worker
// thread; the model only ever mutates on the UI thread.
class DirModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        SizeBytesRole = Qt::UserRole + 1,
        SizeTextRole,
        ModifiedRole,
        ModifiedTextRole,
        IsDirRole,
        UriRole,
    };

    enum Column { NameColumn = 0, SizeColumn = 1, DateColumn = 2 };

    explicit DirModel(QObject *parent = nullptr);
    ~DirModel() override;

    QString uri() const { return m_uri; }
    void setUri(const QString &uri);
    void refresh();

    bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool show);

    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);

    bool flat() const { return m_flat; }
    void setFlat(bool flat);

    int sortColumn() const { return m_sortColumn; }
    Qt::SortOrder sortOrder() const { return m_sortOrder; }
    void setSort(int column, Qt::SortOrder order);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    const Entry &entry(int row) const { return m_entries.at(row); }
    int indexOf(const QString &name) const;

    // Drag and drop: dragging out of a lister uses a private MIME type that
    // keeps the URI, and text/uri-list so other applications can accept it.
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::DropActions supportedDropActions() const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                      const QModelIndex &parent) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    QString lastError() const { return m_lastError; }

signals:
    void uriChanged(const QString &uri);
    void loaded(int count);
    // A drop landed on this listing. The window runs the transfer as a task, so
    // the model never blocks on a copy of several gigabytes.
    void transferRequested(const QVector<FileOps::CopyItem> &items, const QString &targetUri,
                           bool moveFiles);
    void sortChanged(int column, Qt::SortOrder order);
    void loadFailed(const QString &error);

private slots:
    void applyScan(const QString &uri, quint64 token, const QVector<Entry> &entries);
    void applyFailure(const QString &uri, quint64 token, const QString &error);

private:
    void startScan();
    void sortEntries();
    void rebuildVisible();

    QString m_uri;
    QString m_lastError;
    QVector<Entry> m_entries;
    QVector<int> m_visible;
    QString m_filter;
    quint64 m_token = 0;
    bool m_showHidden = true;
    bool m_flat = false;
    int m_sortColumn = NameColumn;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;

    QThread m_thread;
    DirWorker *m_worker = nullptr;
};
