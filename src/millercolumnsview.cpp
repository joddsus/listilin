#include "millercolumnsview.h"

#include "dirmodel.h"
#include "listerview.h"
#include "vfs.h"
#include "vfs.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

MillerColumnsView::MillerColumnsView(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_container = new QWidget(m_scroll);
    m_layout = new QHBoxLayout(m_container);
    m_layout->setContentsMargins(4, 4, 4, 4);
    m_layout->setSpacing(6);
    m_layout->addStretch(1);
    m_scroll->setWidget(m_container);

    outer->addWidget(m_scroll);
}

void MillerColumnsView::setRootUri(const QString &uri)
{
    if (uri == m_rootUri && !m_columns.isEmpty())
        return;
    m_rootUri = uri;
    rebuild();
}

void MillerColumnsView::rebuild()
{
    // Drop the old columns and their models.
    for (const Column &column : m_columns) {
        if (column.view)
            column.view->deleteLater();
        if (column.model)
            column.model->deleteLater();
    }
    m_columns.clear();

    // Remove everything except the trailing stretch.
    while (m_layout->count() > 1) {
        QLayoutItem *item = m_layout->takeAt(0);
        if (item && item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (!m_rootUri.isEmpty())
        addColumn(m_rootUri);
}

void MillerColumnsView::addColumn(const QString &uri)
{
    Column column;
    column.uri = uri;
    column.model = new DirModel(this);
    column.view = new ListerView(this);
    column.view->setModel(column.model);
    column.view->setMinimumWidth(200);
    column.view->setMaximumWidth(260);

    const int index = m_columns.size();
    connect(column.view, &ListerView::itemActivated, this, [this, index](const QModelIndex &) {
        if (index < 0 || index >= m_columns.size())
            return;
        const DirModel *model = m_columns.at(index).model;
        const int row = m_columns.at(index).view->currentIndex().row();
        if (!model || row < 0 || row >= model->rowCount())
            return;
        const QString name = model->data(model->index(row, 0), Qt::DisplayRole).toString();
        const bool isDir = model->data(model->index(row, 0), DirModel::IsDirRole).toBool();
        const QString itemUri = VfsUri::childUri(m_columns.at(index).uri, name);
        if (isDir) {
            emit directoryActivated(itemUri);
            navigate(index, name);
        } else {
            emit fileActivated(itemUri);
        }
    });

    m_columns.append(column);
    m_layout->insertWidget(m_layout->count() - 1, column.view);
    column.model->setUri(uri);
}

void MillerColumnsView::navigate(int columnIndex, const QString &name)
{
    if (columnIndex < 0 || columnIndex >= m_columns.size())
        return;
    const QString childUri = VfsUri::childUri(m_columns.at(columnIndex).uri, name);

    // Drop every column to the right of the selected one.
    while (m_columns.size() > columnIndex + 1) {
        Column removed = m_columns.takeLast();
        if (removed.view)
            removed.view->deleteLater();
        if (removed.model)
            removed.model->deleteLater();
    }
    addColumn(childUri);
}