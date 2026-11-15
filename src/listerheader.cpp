#include "listerheader.h"
#include "dirmodel.h"
#include "listerdelegate.h"
#include "listerview.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>

ListerHeader::ListerHeader(ListerView *view, QWidget *parent)
    : QWidget(parent)
    , m_view(view)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(fontMetrics().height() + 8);
    setCursor(Qt::PointingHandCursor);

    if (m_view) {
        m_view->viewport()->installEventFilter(this);
        connect(m_view->verticalScrollBar(), &QAbstractSlider::rangeChanged, this,
                [this](int, int) { update(); });
    }
}

void ListerHeader::setSortIndicator(int column, Qt::SortOrder order)
{
    m_sortColumn = column;
    m_sortOrder = order;
    update();
}

void ListerHeader::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    const int width = m_view ? m_view->viewport()->width() : this->width();
    const int rowHeight = height();

    QColor textColor = palette().color(QPalette::WindowText);
    textColor.setAlpha(m_view && m_view->hasFocus() ? 210 : 150);
    painter.setPen(textColor);

    const QStringList titles = {tr("Name"), tr("Size"), tr("Modified")};
    const Qt::Alignment alignments = Qt::AlignVCenter;

    for (int column = 0; column < 3; ++column) {
        const QRect rect = ListerDelegate::columnRect(column, width, rowHeight);
        const Qt::Alignment alignment = column == DirModel::NameColumn
            ? (alignments | Qt::AlignLeft)
            : (alignments | Qt::AlignRight);

        QString title = titles.at(column);
        if (column == m_sortColumn)
            title += m_sortOrder == Qt::AscendingOrder ? QStringLiteral(" ↑")
                                                       : QStringLiteral(" ↓");

        painter.drawText(rect, alignment, title);
    }

    painter.setPen(QColor(textColor.red(), textColor.green(), textColor.blue(), 40));
    painter.drawLine(0, rowHeight - 1, width, rowHeight - 1);
}

void ListerHeader::mousePressEvent(QMouseEvent *event)
{
    const int column = columnAt(event->pos().x());
    if (column >= 0)
        emit sortRequested(column);
    QWidget::mousePressEvent(event);
}

void ListerHeader::mouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    QWidget::mouseMoveEvent(event);
}

int ListerHeader::columnAt(int x) const
{
    const int width = m_view ? m_view->viewport()->width() : this->width();
    for (int column = 0; column < 3; ++column) {
        if (ListerDelegate::columnRect(column, width, height()).contains(x, 0))
            return column;
    }
    return -1;
}

bool ListerHeader::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view->viewport() && event->type() == QEvent::Resize)
        update();
    return QWidget::eventFilter(watched, event);
}
