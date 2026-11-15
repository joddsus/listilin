#pragma once

#include <QStyledItemDelegate>

// Paints every pixel of a row. Qt still owns selection, scrolling, keyboard
// navigation and accessibility, so we only have to care about appearance.
class ListerDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ListerDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    static constexpr int padding = 8;
    static constexpr int iconSize = 16;
    static constexpr int iconGap = 6;
    static constexpr int sizeColumnWidth = 84;
    static constexpr int dateColumnWidth = 104;

    // Column geometry lives here so the header and the delegate can never
    // drift apart.
    static QRect columnRect(int column, int width, int height);
};
