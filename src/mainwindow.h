#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QVector>

#include "fileops.h"
#include "filetask.h"

class OperationDialog;
class Panel;
class QMenu;
class QListWidget;
class QToolBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &startPath, QWidget *parent = nullptr);

    Panel *sourcePanel() const { return m_source; }
    Panel *destPanel() const { return m_dest; }
    Panel *activePanel() const { return m_active; }
    Panel *inactivePanel() const;

    void setActivePanel(Panel *panel);

    // Builds one of the two context menus. Separate from showing it so the menu
    // can be inspected (and the switch row tested) without a running popup.
    QMenu *createContextMenu(Panel *panel, bool fileMenu);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void registerCommands();
    void loadBindings();
    void buildButtonBank();
    void buildMenus();
    void buildPlaces();
    void showWelcome();
    void togglePlaces();
    void wirePanel(Panel *panel);
    // fileMenu picks which of the two context menus opens; the other one is one
    // click away through the switch row at the top.
    void showContextMenu(Panel *panel, const QPoint &globalPos, bool fileMenu);
    void restoreSession(const QString &startPath);
    void saveSession();

    void navigateInto(Panel *panel);
    void refreshBoth();
    void viewFiles(bool edit);
    void clipboardCopy(bool cut);
    void clipboardPaste();
    void copyToOtherPanel(bool moveFiles);
    void runTransfer(const QVector<FileOps::CopyItem> &items, const QString &targetUri,
                     bool moveFiles);
    bool startTask(const QString &title, const FileTask::Job &job);
    void taskFinished(const TaskOutcome &outcome);
    bool ensureLocal(Panel *panel);

    Panel *m_source = nullptr;
    Panel *m_dest = nullptr;
    Panel *m_active = nullptr;
    QToolBar *m_toolbar = nullptr;
    QListWidget *m_places = nullptr;

    FileTask *m_task = nullptr;
    QPointer<OperationDialog> m_operation;
    bool m_clipboardCut = false;
};
