#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QTreeWidget;

// Type what you want to do, pick it, done. This is the main reason the command
// registry exists: the palette is generated from it, so nothing can be
// implemented without also becoming discoverable.
class CommandPalette : public QDialog
{
    Q_OBJECT

public:
    explicit CommandPalette(QWidget *parent = nullptr);

    // Shows the palette and returns the chosen command id, or an empty string.
    static QString pick(QWidget *parent);

private:
    void populate(const QString &filter);
    void chooseCurrent();

    QLineEdit *m_search = nullptr;
    QTreeWidget *m_list = nullptr;
    QString m_choice;
};
