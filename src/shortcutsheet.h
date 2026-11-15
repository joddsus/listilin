#pragma once

#include <QDialog>

// Every command and its shortcut, grouped by category. Generated, never
// maintained by hand, so it cannot go stale.
class ShortcutSheet : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutSheet(QWidget *parent = nullptr);

    static void showSheet(QWidget *parent);
};
