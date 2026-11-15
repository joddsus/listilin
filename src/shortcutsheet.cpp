#include "shortcutsheet.h"
#include "command.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QHeaderView>
#include <QMap>
#include <QTreeWidget>
#include <QVBoxLayout>

ShortcutSheet::ShortcutSheet(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    resize(560, 520);

    auto *layout = new QVBoxLayout(this);

    auto *tree = new QTreeWidget(this);
    tree->setColumnCount(3);
    tree->setHeaderLabels({tr("Command"), tr("Shortcut"), tr("Category")});
    tree->setRootIsDecorated(false);
    tree->setAlternatingRowColors(true);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    const CommandRegistry &registry = CommandRegistry::instance();
    for (const Command &command : registry.all()) {
        // "Backspace / Alt+Up": every way in, not just the first one.
        const QStringList sequences = registry.shortcutsFor(command.id);
        if (sequences.isEmpty())
            continue;

        auto *item = new QTreeWidgetItem(tree);
        item->setText(0, command.title);
        item->setText(1, sequences.join(QStringLiteral(" / ")));
        item->setText(2, command.category);
    }

    tree->sortByColumn(2, Qt::AscendingOrder);
    layout->addWidget(tree, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    if (QPushButton *close = buttons->button(QDialogButtonBox::Close))
        close->setText(tr("Close"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void ShortcutSheet::showSheet(QWidget *parent)
{
    ShortcutSheet sheet(parent);
    sheet.exec();
}
