#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QQueue>
#include <QString>
#include <QVector>

#include <QDockWidget>
#include <QTabWidget>

#include "fileops.h"
#include "filetask.h"

// The four Listista workspaces, inherited from Listi and kept in the same
// numeric order as the upstream settings so existing configurations keep their
// meaning.
enum class Workspace
{
    Tviskipt = 0,
    Einn = 1,
    Askr = 2,
    Smidja = 3,
};

class OperationDialog;
class Panel;
class QMenu;
class QListWidget;
class QTreeView;
class QFileSystemModel;
class QDockWidget;
class QToolBar;
class QShortcut;

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
    void showAbout();
    void togglePlaces();
    void applyWorkspace(Workspace workspace);
    void updatePanelVisibility();
    bool singlePaneWorkspace() const;
    Workspace loadWorkspace() const;
    void persistWorkspace();
    void wirePanel(Panel *panel);
    // fileMenu picks which of the two context menus opens; the other one is one
    // click away through the switch row at the top.
    void showContextMenu(Panel *panel, const QPoint &globalPos, bool fileMenu);
    void restoreSession(const QString &startPath);
    void saveSession();

    void navigateInto(Panel *panel);
    void refreshBoth();
    void newTab(const QString &sourceUri = QString());
    void updateTabTitle(int index);
    void closeTab(int index);
    void switchTab(int index);
    void viewFiles(bool edit);
    void findFiles();
    void openPreferences();
    void clipboardCopy(bool cut);
    void clipboardPaste();
    void copyToOtherPanel(bool moveFiles);
    void runTransfer(const QVector<FileOps::CopyItem> &items, const QString &targetUri,
                     bool moveFiles);
    bool startTask(const QString &title, const FileTask::Job &job);
    void startNextTask();
    void taskFinished(const TaskOutcome &outcome);
    int resolveConflict(const QString &source, const QString &target);

    Panel *m_source = nullptr;
    Panel *m_dest = nullptr;
    Panel *m_active = nullptr;
    QToolBar *m_toolbar = nullptr;
    QWidget *m_placesBox = nullptr;
    QListWidget *m_places = nullptr;
    QDockWidget *m_folderTreeDock = nullptr;
    QTreeView *m_folderTree = nullptr;
    QFileSystemModel *m_folderTreeModel = nullptr;

    struct Tab {
        Panel *source = nullptr;
        Panel *dest = nullptr;
        QWidget *page = nullptr;
    };
    QVector<Tab> m_tabs;
    QTabWidget *m_tabWidget = nullptr;

    struct PendingTask
    {
        QString title;
        FileTask::Job job;
    };

    FileTask *m_task = nullptr;
    QQueue<PendingTask> m_pendingTasks;
    int m_conflictMode = 0;
    Workspace m_workspace = Workspace::Tviskipt;
    QVector<QShortcut *> m_globalShortcuts;
    QPointer<OperationDialog> m_operation;
    bool m_clipboardCut = false;
};
