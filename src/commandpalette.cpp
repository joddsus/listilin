#include "commandpalette.h"
#include "command.h"

#include <QHeaderView>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

CommandPalette::CommandPalette(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Command Palette"));
    setModal(true);
    resize(540, 400);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("What do you want to do?"));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    m_list = new QTreeWidget(this);
    m_list->setColumnCount(2);
    m_list->setHeaderHidden(true);
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAllColumnsShowFocus(true);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_list->header()->resizeSection(1, 92);
    layout->addWidget(m_list, 1);

    connect(m_search, &QLineEdit::textChanged, this, &CommandPalette::populate);
    connect(m_search, &QLineEdit::returnPressed, this, &CommandPalette::chooseCurrent);
    connect(m_list, &QTreeWidget::itemActivated, this, &CommandPalette::chooseCurrent);

    populate(QString());
    m_search->setFocus();
}

void CommandPalette::populate(const QString &filter)
{
    m_list->clear();

    const QString needle = filter.trimmed();
    const CommandRegistry &registry = CommandRegistry::instance();

    for (const Command &command : registry.all()) {
        const QString haystack = command.title + QLatin1Char(' ') + command.category
            + QLatin1Char(' ') + command.id;

        if (!needle.isEmpty() && !haystack.contains(needle, Qt::CaseInsensitive))
            continue;

        auto *item = new QTreeWidgetItem(m_list);
        item->setText(0, command.title);
        item->setText(1, registry.shortcutFor(command.id));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setData(0, Qt::UserRole, command.id);
        item->setToolTip(0, QStringLiteral("%1 · %2").arg(command.category, command.id));
    }

    if (m_list->topLevelItemCount() > 0)
        m_list->setCurrentItem(m_list->topLevelItem(0));
}

void CommandPalette::chooseCurrent()
{
    const QTreeWidgetItem *item = m_list->currentItem();
    if (!item)
        return;

    m_choice = item->data(0, Qt::UserRole).toString();
    accept();
}

QString CommandPalette::pick(QWidget *parent)
{
    CommandPalette palette(parent);
    palette.exec();
    return palette.m_choice;
}
