#pragma once

#include <QWidget>

#include <QVector>

#include "millercolumns.h"

class DirModel;
class ListerView;
class QScrollArea;
class QHBoxLayout;

// A Miller-columns browser: each column shows one folder level, and selecting
// a directory opens the next column to its right. Built on the existing
// DirModel/ListerView so archives and remote folders work too.
class MillerColumnsView : public QWidget
{
    Q_OBJECT
public:
    explicit MillerColumnsView(QWidget *parent = nullptr);

    void setRootUri(const QString &uri);

    // The selection in the deepest column that has one. Commands must act on
    // what the Miller browser shows, not on the hidden single-column view.
    QStringList selectedUris() const;
    QString currentName() const;

signals:
    // Emitted when a folder is activated in a column.
    void directoryActivated(const QString &uri);
    // Emitted when a file (not a folder) is activated in a column.
    void fileActivated(const QString &uri);

private:
    struct Column {
        QString uri;
        DirModel *model = nullptr;
        ListerView *view = nullptr;
    };

    void rebuild();
    void addColumn(const QString &uri);
    void navigate(int columnIndex, const QString &name);

    QScrollArea *m_scroll = nullptr;
    QWidget *m_container = nullptr;
    QHBoxLayout *m_layout = nullptr;
    QVector<Column> m_columns;
    QString m_rootUri;
    MillerColumns::Navigator m_navigator;
};