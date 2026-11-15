#pragma once

#include <QListView>
#include <QString>

// A lister. It holds no behaviour of its own: keys are translated into a
// command id and emitted, so the same action can later be a button, a menu
// entry or a script call.
class ListerView : public QListView
{
    Q_OBJECT

public:
    explicit ListerView(QWidget *parent = nullptr);

    QString currentName() const;

signals:
    void commandRequested(const QString &commandId);
    void itemActivated(const QModelIndex &index);
    void contextMenuRequested(const QPoint &globalPos);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
};
