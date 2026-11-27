#pragma once

#include <QHash>
#include <QMainWindow>
#include <QPointer>
#include <QQueue>
#include <QString>
#include <QVector>

#include <QDockWidget>
#include <QTabWidget>

#include <functional>
#include <memory>

#include "fileops.h"
#include "filetask.h"
#include "jobreport.h"
#include "layoutstore.h"
#include "shelf.h"
#include "undostack.h"

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
class ShellWidget;
class QStackedWidget;
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
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void registerCommands();
    void loadBindings();
    void buildButtonBank();
    void buildMenus();
    void buildPlaces();
    void showWelcome();
    void showAbout();
    void togglePlaces();
    void refreshShelfView();
    void refreshOperationsView();
    void syncShellToActivePanel();
    ShellWidget *shellPage(Panel *panel);
    void removeShellPage(Panel *panel);
    void scheduleOperationsRefresh();
    void cancelJob(const QString &id);
    void registerExternalTools();
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
    void compareFolders();
    void openPreferences();
    void clipboardCopy(bool cut);
    void clipboardPaste();
    void copyToOtherPanel(bool moveFiles);
    void runTransfer(const QVector<FileOps::CopyItem> &items, const QString &targetUri,
                     bool moveFiles);
    bool startTask(const QString &title, const FileTask::Job &job,
                   const QString &jobId = QString(),
                   const std::function<void(const TaskOutcome &)> &onDone = {});
    void startNextTask();
    void taskFinished(const TaskOutcome &outcome);
    int resolveConflict(const QString &source, const QString &target, int *mode = nullptr);
    bool undoLast(QString *error = nullptr);

    Panel *m_source = nullptr;
    Panel *m_dest = nullptr;
    Panel *m_active = nullptr;
    QToolBar *m_toolbar = nullptr;
    QWidget *m_placesBox = nullptr;
    QListWidget *m_places = nullptr;
    QDockWidget *m_folderTreeDock = nullptr;
    QTreeView *m_folderTree = nullptr;
    QFileSystemModel *m_folderTreeModel = nullptr;
    QDockWidget *m_shelfDock = nullptr;
    QListWidget *m_shelfList = nullptr;
    QDockWidget *m_operationsDock = nullptr;
    QListWidget *m_operationsList = nullptr;
    QDockWidget *m_shellDock = nullptr;
    QStackedWidget *m_shellStack = nullptr;
    QHash<Panel *, ShellWidget *> m_shellPages;
    QTimer *m_operationsRefreshTimer = nullptr;

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
        QString jobId;
        std::function<void(const TaskOutcome &)> onDone;
    };

    FileTask *m_task = nullptr;
    QQueue<PendingTask> m_pendingTasks;
    std::function<void(const TaskOutcome &)> m_currentCompletion;
    Workspace m_workspace = Workspace::Tviskipt;
    QVector<QShortcut *> m_globalShortcuts;
    QPointer<OperationDialog> m_operation;
    // Identifies files this window put on the clipboard as a cut, so a copy
    // made in another application is never turned into a move.
    QString m_cutToken;

    JobReport::Store m_jobReports;
    Shelf::Store m_shelf;
    UndoStack::Stack m_undoStack;
    QString m_currentJobId;
    int m_lastJobSkipped = 0;
};
