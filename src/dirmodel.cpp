#include "dirmodel.h"
#include "clipboardtofile.h"
#include "dirworker.h"
#include "fileops.h"
#include "format.h"
#include "thumbnails.h"
#include "vfs.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMimeData>
#include <QUrl>

#include <algorithm>

DirModel::DirModel(QObject *parent)
    : QAbstractListModel(parent)
{
    qRegisterMetaType<QVector<Entry>>("QVector<Entry>");

    m_worker = new DirWorker;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &DirWorker::scanned, this, &DirModel::applyScan);
    connect(m_worker, &DirWorker::failed, this, &DirModel::applyFailure);

    // A background thumbnail for a remote or archived file arrived: repaint
    // just that row. The delegate keeps showing the generic icon until then.
    connect(&RemoteThumbnails::instance(), &RemoteThumbnails::ready, this,
            [this](const QString &uri) {
                for (int row = 0; row < m_visible.size(); ++row) {
                    const Entry &entry = m_entries.at(m_visible.at(row));
                    if (VfsUri::childUri(m_uri, entry.name) != uri)
                        continue;
                    const QModelIndex changed = index(row, 0);
                    emit dataChanged(changed, changed);
                }
            });

    m_thread.start();
}

DirModel::~DirModel()
{
    m_thread.quit();
    m_thread.wait();
}

void DirModel::setUri(const QString &uri)
{
    if (uri == m_uri)
        return;

    // Drop the old rows immediately. The scan is asynchronous, and keeping the
    // previous folder's rows visible under the new URI let a command act on a
    // name that belonged to the folder just left.
    beginResetModel();
    m_uri = uri;
    m_entries.clear();
    m_visible.clear();
    m_lastError.clear();
    endResetModel();

    emit uriChanged(m_uri);
    emit loaded(0);
    startScan();
}

void DirModel::refresh()
{
    startScan();
}

void DirModel::setShowHidden(bool show)
{
    if (m_showHidden == show)
        return;
    m_showHidden = show;
    startScan();
}

void DirModel::setFilter(const QString &filter)
{
    if (m_filter == filter)
        return;
    m_filter = filter;
    rebuildVisible();
    beginResetModel();
    endResetModel();
    emit loaded(m_visible.size());
}

void DirModel::setFlat(bool flat)
{
    if (m_flat == flat)
        return;
    m_flat = flat;
    startScan();
}

void DirModel::setSort(int column, Qt::SortOrder order)
{
    if (m_sortColumn == column && m_sortOrder == order) {
        // Clicking the same column flips the direction.
        order = order == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder;
    }

    m_sortColumn = column;
    m_sortOrder = order;
    sortEntries();
    emit sortChanged(m_sortColumn, m_sortOrder);
}

void DirModel::startScan()
{
    if (m_uri.isEmpty())
        return;

    const quint64 token = ++m_token;
    QMetaObject::invokeMethod(m_worker, "scan", Qt::QueuedConnection, Q_ARG(QString, m_uri),
                              Q_ARG(quint64, token), Q_ARG(bool, m_showHidden),
                              Q_ARG(bool, m_flat));
}

void DirModel::applyScan(const QString &uri, quint64 token, const QVector<Entry> &entries)
{
    if (token != m_token || uri != m_uri)
        return;

    beginResetModel();
    m_entries = entries;
    m_lastError.clear();
    sortEntries();
    rebuildVisible();
    endResetModel();

    emit loaded(m_entries.size());
}

void DirModel::applyFailure(const QString &uri, quint64 token, const QString &error)
{
    if (token != m_token || uri != m_uri)
        return;

    beginResetModel();
    m_entries.clear();
    endResetModel();

    m_lastError = error;
    emit loaded(0);
    emit loadFailed(error);
}

void DirModel::sortEntries()
{
    const int column = m_sortColumn;
    const Qt::SortOrder order = m_sortOrder;

    const auto keyLess = [column](const Entry &a, const Entry &b) {
        switch (column) {
        case DirModel::SizeColumn:
            return a.size < b.size;
        case DirModel::DateColumn:
            return a.modified < b.modified;
        default:
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        }
    };

    std::stable_sort(m_entries.begin(), m_entries.end(), [order, keyLess](const Entry &a,
                                                                         const Entry &b) {
        if (a.isDir != b.isDir)
            return a.isDir; // directories always first

        const bool aBefore = keyLess(a, b);
        const bool bBefore = keyLess(b, a);

        if (aBefore == bBefore) // equal keys: fall back to name order
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;

        return order == Qt::AscendingOrder ? aBefore : bBefore;
    });

    rebuildVisible();
}

void DirModel::rebuildVisible()
{
    m_visible.clear();
    m_visible.reserve(m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_filter.isEmpty()
            || m_entries.at(i).name.contains(m_filter, Qt::CaseInsensitive))
            m_visible.append(i);
    }
}

int DirModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_visible.size();
}

QVariant DirModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size())
        return {};

    const Entry &entry = m_entries.at(m_visible.at(index.row()));

    switch (role) {
    case Qt::DisplayRole:
        return entry.name;
    case SizeBytesRole:
        return entry.size;
    case SizeTextRole:
        return entry.isDir ? QString() : humanSize(entry.size);
    case ModifiedRole:
        return entry.modified;
    case ModifiedTextRole:
        return shortDate(entry.modified);
    case IsDirRole:
        return entry.isDir;
    case UriRole:
        return VfsUri::childUri(m_uri, entry.name);
    default:
        return {};
    }
}

QStringList DirModel::mimeTypes() const
{
    return {QStringLiteral("application/x-lister-items"), QStringLiteral("text/uri-list")};
}

QMimeData *DirModel::mimeData(const QModelIndexList &indexes) const
{
    auto *data = new QMimeData;
    QStringList lines;
    QList<QUrl> urls;

    for (const QModelIndex &index : indexes) {
        if (!index.isValid() || index.column() != 0)
            continue;

        const QString uri = index.data(UriRole).toString();
        const bool isDir = index.data(IsDirRole).toBool();
        lines.append((isDir ? QStringLiteral("d|") : QStringLiteral("f|")) + uri);

        if (Vfs::isLocal(uri))
            urls.append(QUrl::fromLocalFile(Vfs::localPathOf(uri)));
    }

    data->setData(QStringLiteral("application/x-lister-items"), lines.join(QLatin1Char('\n')).toUtf8());
    if (!urls.isEmpty())
        data->setUrls(urls);

    return data;
}

Qt::DropActions DirModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags DirModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags base = QAbstractListModel::flags(index);
    base |= Qt::ItemIsDragEnabled;
    if (!index.isValid())
        base |= Qt::ItemIsDropEnabled;

    if (index.isValid() && index.data(IsDirRole).toBool())
        base |= Qt::ItemIsDropEnabled;

    return base;
}

bool DirModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                            const QModelIndex &parent)
{
    Q_UNUSED(row)
    Q_UNUSED(column)

    if (!Vfs::isLocal(m_uri))
        return false;

    // Dropping on a folder row means dropping into that folder.
    QString targetUri = m_uri;
    if (parent.isValid() && parent.data(IsDirRole).toBool())
        targetUri = VfsUri::childUri(m_uri, parent.data(Qt::DisplayRole).toString());

    // A dropped image (from a browser or another application) becomes a PNG in
    // the target folder instead of being ignored.
    if (data->hasImage() && !data->hasUrls()) {
        const QImage image = qvariant_cast<QImage>(data->imageData());
        QString written;
        QString error;
        if (!ClipboardToFile::saveImage(VfsUri::parse(targetUri).toLocalPath(), image, &written,
                                        &error))
            return false;
        refresh();
        return true;
    }

    QVector<FileOps::CopyItem> items;

    const QByteArray custom = data->data(QStringLiteral("application/x-lister-items"));
    if (!custom.isEmpty()) {
        const QStringList lines = QString::fromUtf8(custom).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const int bar = line.indexOf(QLatin1Char('|'));
            if (bar < 1)
                continue;
            FileOps::CopyItem item;
            item.isDir = line.left(bar) == QLatin1String("d");
            item.uri = line.mid(bar + 1);
            items.append(item);
        }
    } else if (data->hasUrls()) {
        const QList<QUrl> urls = data->urls();
        for (const QUrl &url : urls) {
            if (!url.isLocalFile())
                continue;
            FileOps::CopyItem item;
            item.uri = url.toString();
            item.isDir = QFileInfo(url.toLocalFile()).isDir();
            items.append(item);
        }
    }

    if (items.isEmpty())
        return false;

    // Hand the work to the window: a copy belongs in a task with progress and a
    // cancel button, not in the event loop of the model.
    emit transferRequested(items, targetUri, action == Qt::MoveAction);
    return true;
}

int DirModel::indexOf(const QString &name) const
{
    for (int i = 0; i < m_visible.size(); ++i) {
        if (m_entries.at(m_visible.at(i)).name == name)
            return i;
    }
    return -1;
}
