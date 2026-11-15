#include "listerview.h"
#include "command.h"
#include "listerdelegate.h"

#include <QContextMenuEvent>
#include <QKeyEvent>

ListerView::ListerView(QWidget *parent)
    : QListView(parent)
{
    setUniformItemSizes(true);
    setAlternatingRowColors(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setWordWrap(false);
    setResizeMode(QListView::Adjust);
    setLayoutMode(QListView::Batched);
    setBatchSize(2000);
    setItemDelegate(new ListerDelegate(this));

    // Drag files between the two panels (copy by default, move with Shift).
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::CopyAction);

    connect(this, &QAbstractItemView::activated, this, &ListerView::itemActivated);
}

QString ListerView::currentName() const
{
    const QModelIndex index = currentIndex();
    return index.isValid() ? index.data(Qt::DisplayRole).toString() : QString();
}

void ListerView::keyPressEvent(QKeyEvent *event)
{
    const QKeySequence sequence(event->keyCombination());
    const QString commandId = CommandRegistry::instance().commandFor(sequence);
    if (!commandId.isEmpty()) {
        emit commandRequested(commandId);
        event->accept();
        return;
    }

    QListView::keyPressEvent(event);
}

void ListerView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QListView::mouseDoubleClickEvent(event);
}

void ListerView::contextMenuEvent(QContextMenuEvent *event)
{
    QPoint pos = event->pos();
    QPoint global = event->globalPos();

    // Shift+F10 has no cursor on a row: the current item is what a person means
    // by "this file", so the menu opens there instead of on empty space.
    if (event->reason() == QContextMenuEvent::Keyboard && currentIndex().isValid()) {
        pos = visualRect(currentIndex()).center();
        global = viewport()->mapToGlobal(pos);
    }

    const QModelIndex index = indexAt(pos);
    if (index.isValid() && !selectionModel()->isSelected(index))
        setCurrentIndex(index);

    emit contextMenuRequested(global);
    event->accept();
}
