#include "listerdelegate.h"
#include "dirmodel.h"
#include "vfs.h"

#include <QFileIconProvider>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHash>
#include <QIcon>
#include <QPainter>

namespace {

QIcon iconFor(const QModelIndex &index)
{
    static QFileIconProvider provider;
    static QHash<QString, QIcon> cache;

    const bool isDir = index.data(DirModel::IsDirRole).toBool();
    const QString name = index.data(Qt::DisplayRole).toString();
    const QString key = isDir ? QStringLiteral(":dir")
                              : QStringLiteral(":") + QFileInfo(name).suffix().toLower();

    const auto cached = cache.constFind(key);
    if (cached != cache.constEnd())
        return *cached;

    // Ask the platform for one representative icon per type instead of one per
    // row; that keeps painting cheap in huge directories.
    const QString uri = index.data(DirModel::UriRole).toString();

    QIcon icon;
    if (isDir) {
        icon = provider.icon(QFileIconProvider::Folder);
    } else if (Vfs::isLocal(uri)) {
        icon = provider.icon(QFileInfo(Vfs::localPathOf(uri)));
        if (icon.isNull())
            icon = provider.icon(QFileIconProvider::File);
    } else {
        // Remote and archive entries have no local file to ask about, so they
        // share one generic file icon per type.
        icon = provider.icon(QFileIconProvider::File);
    }

    cache.insert(key, icon);
    return icon;
}

} // namespace

ListerDelegate::ListerDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QRect ListerDelegate::columnRect(int column, int width, int height)
{
    switch (column) {
    case DirModel::SizeColumn:
        return QRect(width - dateColumnWidth - sizeColumnWidth - padding, 0, sizeColumnWidth,
                     height);
    case DirModel::DateColumn:
        return QRect(width - dateColumnWidth - padding, 0, dateColumnWidth, height);
    default: {
        const int nameWidth = width - sizeColumnWidth - dateColumnWidth - 2 * padding - iconSize
            - iconGap;
        return QRect(padding + iconSize + iconGap, 0, qMax(0, nameWidth), height);
    }
    }
}

QSize ListerDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QSize(0, option.fontMetrics.height() + 8);
}

void ListerDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                           const QModelIndex &index) const
{
    painter->save();

    const QRect rect = option.rect;
    const bool selected = option.state & QStyle::State_Selected;
    const bool isDir = index.data(DirModel::IsDirRole).toBool();

    if (selected) {
        painter->fillRect(rect, option.palette.highlight());
    } else if (index.row() % 2 == 1) {
        QColor stripe = option.palette.base().color();
        stripe = stripe.lightnessF() > 0.5 ? stripe.darker(103) : stripe.lighter(112);
        painter->fillRect(rect, stripe);
    }

    QColor textColor = selected ? option.palette.highlightedText().color()
                                : option.palette.text().color();
    QColor dimColor = textColor;
    dimColor.setAlpha(selected ? 190 : 145);

    const QRect nameRect = columnRect(DirModel::NameColumn, rect.width(), rect.height())
                               .translated(rect.left(), rect.top());
    const QRect sizeRect = columnRect(DirModel::SizeColumn, rect.width(), rect.height())
                               .translated(rect.left(), rect.top());
    const QRect dateRect = columnRect(DirModel::DateColumn, rect.width(), rect.height())
                               .translated(rect.left(), rect.top());

    // Icon sits in the fixed gutter left of the name column.
    const QRect iconRect(rect.left() + padding, rect.top() + (rect.height() - iconSize) / 2,
                         iconSize, iconSize);
    const QIcon icon = iconFor(index);
    if (!icon.isNull())
        icon.paint(painter, iconRect, Qt::AlignCenter, selected ? QIcon::Selected : QIcon::Normal);

    QFont nameFont = option.font;
    nameFont.setBold(isDir);
    painter->setFont(nameFont);
    painter->setPen(textColor);

    const QFontMetrics nameMetrics(nameFont);
    const QString name = index.data(Qt::DisplayRole).toString();
    painter->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
                      nameMetrics.elidedText(name, Qt::ElideMiddle, nameRect.width()));

    painter->setFont(option.font);
    painter->setPen(dimColor);
    painter->drawText(sizeRect, Qt::AlignVCenter | Qt::AlignRight,
                      index.data(DirModel::SizeTextRole).toString());
    painter->drawText(dateRect, Qt::AlignVCenter | Qt::AlignRight,
                      index.data(DirModel::ModifiedTextRole).toString());

    painter->restore();
}
