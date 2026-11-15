#pragma once

#include <QWidget>

class ListerView;

// Column labels drawn at the same offsets as ListerDelegate uses, so the
// header stays aligned with the delegate's own layout instead of a second,
// drifting definition of the columns.
class ListerHeader : public QWidget
{
    Q_OBJECT

public:
    explicit ListerHeader(ListerView *view, QWidget *parent = nullptr);

    void setSortIndicator(int column, Qt::SortOrder order);

signals:
    void sortRequested(int column);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    int columnAt(int x) const;

    ListerView *m_view = nullptr;
    int m_sortColumn = 0;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};
