#include "mainwindow.h"
#include "command.h"
#include "commandpalette.h"
#include "dirmodel.h"
#include "fileops.h"
#include "filelistexport.h"
#include "temporarycopy.h"
#include "filetask.h"
#include "searchtask.h"
#include "foldercompare.h"
#include "operationdialog.h"
#include "shortcutsheet.h"
#include "listerview.h"
#include "panel.h"
#include "settingsafety.h"
#include "shellwidget.h"
#include "terminallauncher.h"
#include "clipboardtofile.h"
#include "textdiff.h"
#include "pathsafety.h"
#include "archivepaths.h"
#include "propertiesbatch.h"
#include "remoteuri.h"
#include "sshauth.h"
#include "vfs_curl.h"
#include "duplicatefinder.h"
#include "externaltools.h"
#include "filehash.h"
#include "fileshred.h"
#include "imagedrop.h"
#include "renamebatch.h"
#include "transferplan.h"
#include "treesnapshot.h"
#include "vfs.h"
#include "vfs_archive.h"

#include <QUuid>

#include "format.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QThread>
#include <QClipboard>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEventLoop>
#include <QRegularExpression>
#include <QMimeData>
#include <QMap>
#include <QVariant>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QProgressDialog>
#include <QDirIterator>
#include <QKeySequenceEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTextEdit>
#include <QMimeDatabase>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>
#include <QComboBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QDockWidget>
#include <QFutureWatcher>
#include <QSet>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStorageInfo>
#include <QStyle>
#include <QVBoxLayout>
#include <functional>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QPushButton>
#include <QUrl>
#include <QtConcurrent>

#include <atomic>
#include <memory>

namespace {

// The button bank is data: ids only. Everything a button can do is a command,
// which is why the same list can be edited in a config file.
const char *const kButtonBank[] = {
    "lister.historyBack", "lister.historyForward", "lister.parent", "lister.home",
    "lister.refresh",     "-",                     "file.mkdir",    "file.copy",
    "file.move",          "file.trash",            "-",             "lister.toggleHidden",
};

QString workspaceId(Workspace workspace)
{
    switch (workspace) {
    case Workspace::Einn:
        return QStringLiteral("einn");
    case Workspace::Askr:
        return QStringLiteral("askr");
    case Workspace::Smidja:
        return QStringLiteral("smidja");
    case Workspace::Tviskipt:
        break;
    }
    return QStringLiteral("tviskipt");
}

QString workspaceName(Workspace workspace)
{
    switch (workspace) {
    case Workspace::Einn:
        return QStringLiteral("Einn");
    case Workspace::Askr:
        return QStringLiteral("Askr");
    case Workspace::Smidja:
        return QStringLiteral("Smiðja");
    case Workspace::Tviskipt:
        break;
    }
    return QStringLiteral("Tvískipt");
}

QString workspaceMenuLabel(Workspace workspace)
{
    switch (workspace) {
    case Workspace::Einn:
        return QStringLiteral("Einn: simple single-panel browsing");
    case Workspace::Askr:
        return QStringLiteral("Askr: folder tree and file list");
    case Workspace::Smidja:
        return QStringLiteral("Smiðja: workbench with more tools");
    case Workspace::Tviskipt:
        break;
    }
    return QStringLiteral("Tvískipt: side-by-side file panels");
}

// Right-click menus, also data. There are two of them because they answer two
// different questions: "what can I do with this file" and "what can I do with
// this folder". Both are one click apart, and the switch is the first row, so
// landing on a file never hides the folder commands.
const QStringList &fileContextMenu()
{
    static const QStringList ids = {
        QStringLiteral("lister.open"),    QStringLiteral("tab.openFolder"),
        QStringLiteral("file.view"),      QStringLiteral("file.edit"),
        QStringLiteral("file.rename"),    QStringLiteral("file.renameBatch"),
        QStringLiteral("selection.sameType"), QStringLiteral("file.collectToShelf"),
        QStringLiteral("-"),             QStringLiteral("file.copy"),
                QStringLiteral("file.move"),     QStringLiteral("file.clipboardCopy"),
        QStringLiteral("file.clipboardPaste"), QStringLiteral("file.clipboardToFile"),
        QStringLiteral("file.copyPath"), QStringLiteral("file.exportList"),
        QStringLiteral("file.compress"),
        QStringLiteral("file.extract"),   QStringLiteral("file.trash"),     QStringLiteral("file.properties"),
    };
    return ids;
}

const QStringList &folderContextMenu()
{
    static const QStringList ids = {
        QStringLiteral("file.newFile"),        QStringLiteral("file.mkdir"),
        QStringLiteral("tab.openFolder"),
        QStringLiteral("lister.refresh"),      QStringLiteral("-"),
        QStringLiteral("lister.toggleHidden"), QStringLiteral("lister.focusPath"),
        QStringLiteral("view.togglePlaces"),   QStringLiteral("-"),
        QStringLiteral("panel.next"),
    };
    return ids;
}

QIcon iconForCommand(const QString &id)
{
    static const QHash<QString, QString> themeNames = {
        {QStringLiteral("lister.historyBack"), QStringLiteral("go-previous")},
        {QStringLiteral("lister.historyForward"), QStringLiteral("go-next")},
        {QStringLiteral("lister.parent"), QStringLiteral("go-up")},
        {QStringLiteral("lister.home"), QStringLiteral("go-home")},
        {QStringLiteral("lister.refresh"), QStringLiteral("view-refresh")},
        {QStringLiteral("file.mkdir"), QStringLiteral("folder-new")},
        {QStringLiteral("file.copy"), QStringLiteral("edit-copy")},
        {QStringLiteral("file.move"), QStringLiteral("edit-cut")},
        {QStringLiteral("file.trash"), QStringLiteral("user-trash")},
        {QStringLiteral("lister.toggleHidden"), QStringLiteral("view-hidden")},
    };

    static const QHash<QString, QStyle::StandardPixmap> fallbacks = {
        {QStringLiteral("lister.historyBack"), QStyle::SP_ArrowBack},
        {QStringLiteral("lister.historyForward"), QStyle::SP_ArrowForward},
        {QStringLiteral("lister.parent"), QStyle::SP_ArrowUp},
        {QStringLiteral("lister.home"), QStyle::SP_DirHomeIcon},
        {QStringLiteral("lister.refresh"), QStyle::SP_BrowserReload},
        {QStringLiteral("file.mkdir"), QStyle::SP_FileDialogNewFolder},
        {QStringLiteral("file.copy"), QStyle::SP_FileIcon},
        {QStringLiteral("file.move"), QStyle::SP_ArrowRight},
        {QStringLiteral("file.trash"), QStyle::SP_TrashIcon},
        {QStringLiteral("lister.toggleHidden"), QStyle::SP_FileDialogContentsView},
    };

    QIcon icon = QIcon::fromTheme(themeNames.value(id));
    if (!icon.isNull())
        return icon;

    const auto fallback = fallbacks.constFind(id);
    if (fallback != fallbacks.constEnd())
        return QApplication::style()->standardIcon(*fallback);

    return {};
}

QString archivePasswordPrompt(const QString &path)
{
    auto ask = [&]() {
        bool ok = false;
        return QInputDialog::getText(nullptr, QObject::tr("Password"),
                                     QObject::tr("Enter password for %1").arg(QFileInfo(path).fileName()),
                                     QLineEdit::Password, QString(), &ok);
    };

    // The prompt can be triggered by a worker thread while listing an archive.
    // Qt widgets must only be touched on the GUI thread, so marshal the dialog
    // there and block until the user answers (or cancels).
    if (QThread::currentThread() == QCoreApplication::instance()->thread())
        return ask();

    QString result;
    QMetaObject::invokeMethod(QCoreApplication::instance(), [&]() { result = ask(); },
                              Qt::BlockingQueuedConnection);
    return result;
}

} // namespace

MainWindow::MainWindow(const QString &startPath, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Listista"));

    // SFTP key settings are process-wide so they also apply to saved session
    // URIs that are opened without going through the Connect dialog. The
    // passphrase is deliberately not stored on disk.
    {
        const QSettings sshSettings;
        SshAuth::Options ssh;
        ssh.identityFile = sshSettings.value(QStringLiteral("remote/sshIdentityFile")).toString();
        ssh.knownHostsFile = sshSettings.value(QStringLiteral("remote/sshKnownHosts")).toString();
        ssh.publicKeyFile = SshAuth::publicKeyFor(ssh.identityFile);
        setSshOptions(ssh);
    }

    // Tabs let several folder pairs stay open at once. Each tab owns its own
    // source/destination pair; m_source/m_dest always point at the active one.
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setTabsClosable(true);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::switchTab);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

    // A column of places on the left, because "where can I go" is the first
    // question a new user has.
    auto *placesBox = new QWidget(this);
    m_placesBox = placesBox;
    auto *placesLayout = new QVBoxLayout(placesBox);
    placesLayout->setContentsMargins(0, 0, 0, 0);
    placesLayout->setSpacing(0);

    auto *placesTitle = new QLabel(tr("Places"), placesBox);
    placesTitle->setContentsMargins(12, 8, 8, 4);
    QFont placesFont = placesTitle->font();
    placesFont.setBold(true);
    placesFont.setPointSizeF(placesFont.pointSizeF() * 0.9);
    placesTitle->setFont(placesFont);
    placesLayout->addWidget(placesTitle);

    m_places = new QListWidget(placesBox);
    m_places->setObjectName(QStringLiteral("places"));
    m_places->setFrameShape(QFrame::NoFrame);
    m_places->setMinimumWidth(150);
    m_places->setMaximumWidth(215);
    m_places->setIconSize(QSize(16, 16));
    m_places->setAlternatingRowColors(false);
    m_places->setUniformItemSizes(true);
    placesLayout->addWidget(m_places, 1);

    auto *outer = new QSplitter(Qt::Horizontal, this);
    outer->setChildrenCollapsible(false);
    outer->setHandleWidth(1);
    outer->addWidget(placesBox);
    outer->addWidget(m_tabWidget);
    outer->setStretchFactor(0, 0);
    outer->setStretchFactor(1, 1);
    outer->setSizes({180, 1000});
    setCentralWidget(outer);

    // Optional folder tree: a second way to navigate, kept as a dock so it
    // never steals space from the two panels by default.
    m_folderTreeDock = new QDockWidget(tr("Folder tree"), this);
    m_folderTreeDock->setObjectName(QStringLiteral("folderTree"));
    m_folderTreeModel = new QFileSystemModel(m_folderTreeDock);
    m_folderTreeModel->setRootPath(QDir::homePath());
    m_folderTree = new QTreeView(m_folderTreeDock);
    m_folderTree->setModel(m_folderTreeModel);
    m_folderTree->setRootIndex(m_folderTreeModel->index(QDir::homePath()));
    m_folderTree->setHeaderHidden(true);
    m_folderTreeDock->setWidget(m_folderTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_folderTreeDock);
    m_folderTreeDock->hide();
    connect(m_folderTree, &QTreeView::clicked, this, [this](const QModelIndex &index) {
        const QString path = m_folderTreeModel->filePath(index);
        if (QFileInfo(path).isDir())
            activePanel()->setUri(QUrl::fromLocalFile(path).toString());
    });

    // The session Shelf collects entries from any panel. It is a dock so it
    // can stay open beside the panels without taking a column from them.
    m_shelfDock = new QDockWidget(tr("Shelf"), this);
    m_shelfDock->setObjectName(QStringLiteral("shelf"));
    m_shelfList = new QListWidget(m_shelfDock);
    m_shelfList->setObjectName(QStringLiteral("shelfList"));
    m_shelfList->setFrameShape(QFrame::NoFrame);
    m_shelfDock->setWidget(m_shelfList);
    m_shelfList->setAcceptDrops(true);
    m_shelfList->viewport()->setAcceptDrops(true);
    m_shelfList->installEventFilter(this);
    m_shelfList->viewport()->installEventFilter(this);
    addDockWidget(Qt::RightDockWidgetArea, m_shelfDock);
    m_shelfDock->hide();
    connect(m_shelfList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item) {
                if (!item)
                    return;
                const QString uri = item->data(Qt::UserRole).toString();
                if (uri.isEmpty())
                    return;
                Panel *panel = activePanel();
                if (item->data(Qt::UserRole + 1).toBool()) {
                    panel->setUri(uri);
                } else {
                    panel->setUri(VfsUri::parentUri(uri));
                    panel->selectName(vfsLeafName(uri));
                }
                panel->focusLister();
            });
    auto *removeFromShelf = new QShortcut(QKeySequence::Delete, m_shelfList);
    removeFromShelf->setContext(Qt::WidgetWithChildrenShortcut);
    connect(removeFromShelf, &QShortcut::activated, this, [this]() {
        const int row = m_shelfList->currentRow();
        if (row < 0)
            return;
        m_shelf.removeAt(row);
        refreshShelfView();
    });

    // Operations dock: a live list of job reports, so waiting/running/
    // completed/failed/cancelled transfers can be watched without opening
    // the report dialog.
    m_operationsDock = new QDockWidget(tr("Operations"), this);
    m_operationsDock->setObjectName(QStringLiteral("operations"));
    auto *operationsContainer = new QWidget(m_operationsDock);
    auto *operationsLayout = new QVBoxLayout(operationsContainer);
    operationsLayout->setContentsMargins(4, 4, 4, 4);
    operationsLayout->setSpacing(4);
    m_operationsList = new QListWidget(operationsContainer);
    m_operationsList->setObjectName(QStringLiteral("operationsList"));
    m_operationsList->setFrameShape(QFrame::NoFrame);
    m_operationsList->setContextMenuPolicy(Qt::CustomContextMenu);
    operationsLayout->addWidget(m_operationsList, 1);

    auto *operationsButtons = new QHBoxLayout();
    auto *cancelButton = new QPushButton(tr("Cancel selected"), operationsContainer);
    auto *clearButton = new QPushButton(tr("Clear finished"), operationsContainer);
    operationsButtons->addWidget(cancelButton);
    operationsButtons->addWidget(clearButton);
    operationsButtons->addStretch();
    operationsLayout->addLayout(operationsButtons);
    m_operationsDock->setWidget(operationsContainer);
    addDockWidget(Qt::BottomDockWidgetArea, m_operationsDock);
    m_operationsDock->hide();

    // Per-panel shells: each panel keeps its own command line and scrollback in
    // this dock, so switching panels switches shells without losing either.
    // One shell is created lazily, the first time it is shown.
    m_shellDock = new QDockWidget(tr("Shell"), this);
    m_shellDock->setObjectName(QStringLiteral("shell"));
    m_shellStack = new QStackedWidget(m_shellDock);
    m_shellStack->setMinimumHeight(120);
    m_shellDock->setWidget(m_shellStack);
    addDockWidget(Qt::BottomDockWidgetArea, m_shellDock);
    m_shellDock->hide();

    connect(m_operationsList, &QListWidget::itemDoubleClicked, this, [this]() {
        CommandContext context;
        if (m_active) {
            context.sourcePath = m_active->uri();
            context.destPath = m_active->uri();
            context.selection = m_active->selection();
        }
        CommandRegistry::instance().run(QStringLiteral("view.jobReports"), context);
    });
    connect(m_operationsList, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                auto *item = m_operationsList->itemAt(pos);
                if (!item)
                    return;
                QMenu menu(this);
                auto *cancelAction = menu.addAction(tr("Cancel"));
                auto *clearAction = menu.addAction(tr("Clear finished"));
                const QString id = item->data(Qt::UserRole).toString();
                const bool cancellable = !id.isEmpty();
                cancelAction->setEnabled(cancellable);
                const QAction *chosen = menu.exec(m_operationsList->viewport()->mapToGlobal(pos));
                if (chosen == cancelAction)
                    cancelJob(id);
                else if (chosen == clearAction)
                    m_jobReports.clearCompleted();
                refreshOperationsView();
            });
    connect(cancelButton, &QPushButton::clicked, this, [this]() {
        if (auto *item = m_operationsList->currentItem())
            cancelJob(item->data(Qt::UserRole).toString());
    });
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        m_jobReports.clearCompleted();
        refreshOperationsView();
        statusBar()->showMessage(tr("Finished job reports cleared."), 2500);
    });

    // Coalesce progress-driven rebuilds of the operations list so a fast
    // transfer does not rebuild it on every poll. Start and finish still rebuild
    // immediately, so the queue and the final state are never delayed.
    m_operationsRefreshTimer = new QTimer(this);
    m_operationsRefreshTimer->setSingleShot(true);
    connect(m_operationsRefreshTimer, &QTimer::timeout, this, [this]() {
        refreshOperationsView();
    });

    // Keep the coalesced timer from firing after the dock is hidden, and make
    // sure the list is rebuilt the moment it is shown again.
    connect(m_operationsDock, &QDockWidget::visibilityChanged, this,
            [this](bool visible) {
                if (!visible && m_operationsRefreshTimer)
                    m_operationsRefreshTimer->stop();
                refreshOperationsView();
            });

    resize(1180, 640);

    newTab();

    m_active = m_source;
    setActivePanel(m_source);
    restoreSession(startPath);

    setArchivePasswordProvider(&archivePasswordPrompt);

    registerCommands();
    registerExternalTools();
    loadBindings();
    buildButtonBank();
    buildMenus();
    buildPlaces();

    const bool icons = QSettings().value(QStringLiteral("ui/iconView"), false).toBool();
    m_source->setIconView(icons);
    m_dest->setIconView(icons);

    if (!QSettings().value(QStringLiteral("ui/welcomeShown"), false).toBool())
        QTimer::singleShot(400, this, &MainWindow::showWelcome);

    // Clicks inside Miller columns and the path bars change focus without a
    // mouse press reaching Panel's own filter; follow focus as well so every
    // click establishes which side toolbar commands act on.
    connect(qApp, &QApplication::focusChanged, this,
            [this](QWidget *, QWidget *now) {
                if (!now)
                    return;
                for (const Tab &tab : m_tabs) {
                    if (tab.source && tab.source->isAncestorOf(now)) {
                        setActivePanel(tab.source);
                        return;
                    }
                    if (tab.dest && tab.dest->isAncestorOf(now)) {
                        setActivePanel(tab.dest);
                        return;
                    }
                }
            });

    // Sessions must survive every exit path, not only the window close button.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveSession);

    statusBar()->showMessage(
        tr("Ctrl+K command palette  ·  Alt+C copy  ·  Alt+M move  ·  Tab switch panel"));
}

void MainWindow::wirePanel(Panel *panel)
{
    // A plain click used to update the lister's selection without telling the
    // window which side was active, so a toolbar command could act on the
    // previously active panel. Mouse and focus now both establish the side.
    connect(panel, &Panel::activationRequested, this,
            [this, panel]() { setActivePanel(panel); });

    connect(panel, &Panel::commandRequested, this, [this, panel](const QString &id) {
        // Make sure m_source/m_dest point at this panel's tab before running.
        for (const Tab &tab : m_tabs) {
            if (tab.source == panel || tab.dest == panel) {
                m_source = tab.source;
                m_dest = tab.dest;
                break;
            }
        }
        setActivePanel(panel);
        CommandContext context;
        context.sourcePath = m_source->uri();
        context.destPath = m_dest->uri();
        context.selection = panel->selection();
        CommandRegistry::instance().run(id, context);
    });

    connect(panel, &Panel::uriChanged, this, [this, panel](const QString &uri) {
        if (panel == m_active)
            setActivePanel(panel);

        if (auto *shell = m_shellPages.value(panel))
            shell->followUri(uri);

        // A tab is named after the folder in its source panel, which is how a
        // row of tabs stays readable once there are more than two.
        for (int i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs.at(i).source == panel) {
                updateTabTitle(i);
                break;
            }
        }
    });

    connect(panel, &Panel::contextMenuRequested, this, [this, panel](const QPoint &globalPos) {
        for (const Tab &tab : m_tabs) {
            if (tab.source == panel || tab.dest == panel) {
                m_source = tab.source;
                m_dest = tab.dest;
                break;
            }
        }
        setActivePanel(panel);

        // Right-clicking an item selects it, so the menu talks about the row
        // under the cursor. Empty space keeps the selection, which is what the
        // "File menu" entry then operates on.
        ListerView *view = panel->view();
        const QModelIndex index = view->indexAt(view->viewport()->mapFromGlobal(globalPos));
        if (index.isValid()) {
            view->setCurrentIndex(index);
            if (!view->selectionModel()->isSelected(index))
                view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
        }

        showContextMenu(panel, globalPos, index.isValid());
    });

    // Dropping into a lister is the same operation as F5, so it goes through the
    // same cancellable task instead of blocking the model.
    connect(panel->model(), &DirModel::transferRequested, this,
            [this](const QVector<FileOps::CopyItem> &items, const QString &targetUri,
                   bool moveFiles) { runTransfer(items, targetUri, moveFiles); });
}

Panel *MainWindow::inactivePanel() const
{
    return m_active == m_source ? m_dest : m_source;
}

void MainWindow::setActivePanel(Panel *panel)
{
    if (!panel)
        return;

    for (Tab &tab : m_tabs) {
        if (tab.source == panel || tab.dest == panel) {
            m_source = tab.source;
            m_dest = tab.dest;
            tab.active = panel;
            break;
        }
    }

    m_active = panel;
    m_source->setActive(panel == m_source);
    m_dest->setActive(panel == m_dest);
    updatePanelVisibility();
    syncShellToActivePanel();
}

ShellWidget *MainWindow::shellPage(Panel *panel)
{
    if (!panel)
        return nullptr;
    if (auto *existing = m_shellPages.value(panel))
        return existing;

    const QString start = Vfs::isLocal(panel->uri()) ? Vfs::localPathOf(panel->uri())
                                                     : QDir::homePath();
    auto *page = new ShellWidget(start, m_shellStack);
    page->followUri(panel->uri());
    m_shellStack->addWidget(page);
    m_shellPages.insert(panel, page);
    connect(panel, &QObject::destroyed, this, [this, panel]() { removeShellPage(panel); });
    return page;
}

void MainWindow::removeShellPage(Panel *panel)
{
    ShellWidget *page = m_shellPages.take(panel);
    if (!page)
        return;
    if (m_shellStack)
        m_shellStack->removeWidget(page);
    page->deleteLater();
}

void MainWindow::syncShellToActivePanel()
{
    if (!m_shellDock || !m_shellDock->isVisible() || !m_active)
        return;
    if (ShellWidget *page = shellPage(m_active))
        m_shellStack->setCurrentWidget(page);
}

void MainWindow::newTab(const QString &sourceUri)
{
    auto *page = new QWidget(m_tabWidget);
    auto *splitter = new QSplitter(Qt::Horizontal, page);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(1);
    auto *source = new Panel(splitter);
    auto *dest = new Panel(splitter);
    splitter->addWidget(source);
    splitter->addWidget(dest);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(splitter);

    Tab tab;
    tab.source = source;
    tab.dest = dest;
    tab.page = page;
    tab.active = source;
    m_tabs.append(tab);

    wirePanel(source);
    wirePanel(dest);

    // A new tab starts where the current one is, so it is a second view of the
    // same place rather than a detour through the home folder.
    if (m_source && m_dest) {
        source->model()->setShowHidden(m_source->model()->showHidden());
        dest->model()->setShowHidden(m_dest->model()->showHidden());
        source->setUri(sourceUri.isEmpty() ? m_source->uri() : sourceUri);
        dest->setUri(m_dest->uri());
        source->setIconView(m_source->iconView());
        dest->setIconView(m_dest->iconView());
    }

    const bool preview = m_workspace == Workspace::Smidja;
    source->setPreviewVisible(preview);
    dest->setPreviewVisible(preview);

    const int index = m_tabWidget->addTab(page, tr("Tab %1").arg(m_tabs.size()));
    m_tabWidget->setCurrentIndex(index);
    switchTab(index);
    updateTabTitle(index);
}

void MainWindow::updateTabTitle(int index)
{
    if (index < 0 || index >= m_tabs.size() || index >= m_tabWidget->count())
        return;

    Panel *panel = m_tabs.at(index).source;
    if (!panel)
        return;

    QString title = vfsLeafName(panel->uri());
    if (title.isEmpty())
        title = tr("Tab %1").arg(index + 1);

    m_tabWidget->setTabText(
        index, m_tabWidget->fontMetrics().elidedText(title, Qt::ElideMiddle, 180));
    m_tabWidget->setTabToolTip(index, panel->uri());
}

void MainWindow::closeTab(int index)
{
    if (m_tabs.size() <= 1)
        return; // keep at least one tab
    if (index < 0 || index >= m_tabs.size())
        return;

    Tab tab = m_tabs.takeAt(index);
    if (tab.source)
        tab.source->deleteLater();
    if (tab.dest)
        tab.dest->deleteLater();
    if (tab.page)
        tab.page->deleteLater();
    m_tabWidget->removeTab(index);

    if (m_tabWidget->count() > 0)
        switchTab(m_tabWidget->currentIndex());
}

void MainWindow::switchTab(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    m_source = m_tabs[index].source;
    m_dest = m_tabs[index].dest;
    if (m_source) {
        // Go back to the side the user last worked in for this tab.
        Panel *target = m_tabs[index].active;
        if (target != m_source && target != m_dest)
            target = m_source;
        setActivePanel(target);
    }
}

void MainWindow::navigateInto(Panel *panel)
{
    if (!panel)
        return;

    const QModelIndex index = panel->view()->currentIndex();
    if (!index.isValid())
        return;

    const QString uri = index.data(DirModel::UriRole).toString();
    const bool isDir = index.data(DirModel::IsDirRole).toBool();

    if (isDir) {
        panel->setUri(uri);
        panel->focusLister();
        return;
    }

    // A zip, tar or 7z is a folder as far as this program is concerned.
    if (VfsUri::looksLikeArchive(uri)) {
        panel->setUri(VfsUri::archiveUriFor(uri));
        panel->focusLister();
        return;
    }

    if (Vfs::isLocal(uri)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(Vfs::localPathOf(uri)));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const QString local = TemporaryCopy::ofUri(uri, &error);
    QApplication::restoreOverrideCursor();

    if (local.isEmpty()) {
        QMessageBox::warning(this, tr("Open"), error);
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(local));
}

void MainWindow::clipboardCopy(bool cut)
{
    Panel *panel = activePanel();
    const QStringList uris = panel->selectedUris();
    if (uris.isEmpty()) {
        statusBar()->showMessage(tr("Select something first."), 4000);
        return;
    }

    QMimeData *mime = new QMimeData();
    QList<QUrl> urls;
    for (const QString &uri : uris) {
        if (Vfs::isLocal(uri))
            urls.append(QUrl::fromLocalFile(Vfs::localPathOf(uri)));
        else
            urls.append(QUrl(uri));
    }
    mime->setUrls(urls);

    // Mark a cut inside the clipboard data itself instead of in a window flag:
    // pasting something copied in another application must never become a
    // move of whatever this window cut earlier.
    m_cutToken.clear();
    if (cut) {
        m_cutToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
        mime->setData(QStringLiteral("application/x-listista-cut"), m_cutToken.toUtf8());
    }

    QApplication::clipboard()->setMimeData(mime);
    statusBar()->showMessage(cut ? tr("Cut to clipboard.") : tr("Copied to clipboard."), 4000);
}

void MainWindow::clipboardPaste()
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!mime || !mime->hasUrls()) {
        statusBar()->showMessage(tr("The clipboard has no files."), 4000);
        return;
    }

    const bool cut = !m_cutToken.isEmpty()
        && mime->hasFormat(QStringLiteral("application/x-listista-cut"))
        && mime->data(QStringLiteral("application/x-listista-cut")) == m_cutToken.toUtf8();

    QVector<FileOps::CopyItem> items;
    for (const QUrl &url : mime->urls()) {
        FileOps::CopyItem item;
        if (url.isLocalFile()) {
            // Build the URI from the path directly. QUrl::toString() would
            // percent-encode spaces and % signs, and the file name would no
            // longer be the one the user selected.
            const QString path = url.toLocalFile();
            item.uri = QStringLiteral("file://") + path;
            item.isDir = QFileInfo(path).isDir() && !QFileInfo(path).isSymLink();
        } else {
            item.uri = url.toString();
            item.isDir = false;
        }
        items.append(item);
    }

    runTransfer(items, activePanel()->uri(), cut);
}

void MainWindow::viewFiles(bool edit)
{
    Panel *panel = activePanel();
    const QStringList uris = panel->selectedUris();
    if (uris.isEmpty()) {
        statusBar()->showMessage(tr("Select something first."), 4000);
        return;
    }

    for (const QString &uri : uris) {
        const QString name = vfsLeafName(uri);

        // Binary or non-text: hand off to the system viewer/editor.
        QMimeDatabase db;
        const QString local = Vfs::isLocal(uri) ? Vfs::localPathOf(uri) : QString();
        bool isText = false;
        if (!local.isEmpty()) {
            const QMimeType mime = db.mimeTypeForFile(local);
            isText = mime.name().startsWith(QStringLiteral("text/"))
                     || mime.name() == QLatin1String("application/json")
                     || mime.name() == QLatin1String("application/xml");
        } else {
            isText = name.endsWith(QLatin1String(".txt")) || name.endsWith(QLatin1String(".md"))
                     || name.endsWith(QLatin1String(".cpp")) || name.endsWith(QLatin1String(".h"))
                     || name.endsWith(QLatin1String(".py")) || name.endsWith(QLatin1String(".js"));
        }

        if (!isText) {
            // Fetch remote/archive entries to a temp file first.
            QString target = local;
            if (target.isEmpty()) {
                QString error;
                target = TemporaryCopy::ofUri(uri, &error);
                if (target.isEmpty()) {
                    QMessageBox::warning(this, tr("Open"), error);
                    continue;
                }
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(target));
            continue;
        }

        // Read the content. A failed read must not open an empty editor that
        // would then save over the original.
        QString content;
        QString tempPath;
        bool needUpload = false;
        bool readable = false;
        QDateTime diskModified;
        qint64 diskSize = -1;
        if (Vfs::isLocal(uri)) {
            const QString path = Vfs::localPathOf(uri);
            QFile f(path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                content = QString::fromUtf8(f.readAll());
                f.close();
                readable = true;
                diskModified = QFileInfo(path).lastModified();
                diskSize = QFileInfo(path).size();
            }
        } else {
            QString error;
            tempPath = TemporaryCopy::ofUri(uri, &error);
            if (tempPath.isEmpty()) {
                QMessageBox::warning(this, tr("Open"), error);
                continue;
            }
            QFile f(tempPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                content = QString::fromUtf8(f.readAll());
                f.close();
                readable = true;
            }
            needUpload = Vfs::instance().writable(VfsUri::parentUri(uri));
        }

        if (!readable) {
            QMessageBox::warning(this, tr("Open"),
                                 tr("Could not read %1. It was not opened, so nothing "
                                    "can overwrite it.")
                                     .arg(name));
            continue;
        }

        // Saving stays open on failure: the text the user typed is never
        // thrown away because a write, an upload or a replaced-on-disk check
        // went wrong.
        const bool canSave = edit && (Vfs::isLocal(uri) || needUpload);
        bool saved = false;
        while (!saved) {
            QDialog dialog(this);
            dialog.setWindowTitle(edit ? tr("Edit — %1").arg(name) : tr("View — %1").arg(name));
            dialog.resize(720, 480);
            auto *layout = new QVBoxLayout(&dialog);
            auto *editor = new QPlainTextEdit(&dialog);
            editor->setPlainText(content);
            editor->setReadOnly(!canSave);
            layout->addWidget(editor);
            if (edit && !canSave) {
                auto *note = new QLabel(tr("This location is read-only; the text can be viewed "
                                          "only."),
                                        &dialog);
                note->setWordWrap(true);
                layout->addWidget(note);
            }
            auto *buttons = new QDialogButtonBox(&dialog);
            if (canSave) {
                buttons->addButton(QDialogButtonBox::Save);
                buttons->addButton(QDialogButtonBox::Cancel);
            } else {
                buttons->addButton(QDialogButtonBox::Close);
            }
            layout->addWidget(buttons);
            connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

            if (dialog.exec() != QDialog::Accepted || !canSave)
                break;

            const QByteArray newData = editor->toPlainText().toUtf8();

            if (Vfs::isLocal(uri)) {
                const QString path = Vfs::localPathOf(uri);
                const QFileInfo now(path);
                if (now.exists()
                    && (now.lastModified() != diskModified || now.size() != diskSize)) {
                    const auto answer = QMessageBox::question(
                        this, tr("Edit — %1").arg(name),
                        tr("%1 changed on disk while it was open. Replace those changes?")
                            .arg(name),
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                    if (answer != QMessageBox::Yes) {
                        content = editor->toPlainText();
                        continue;
                    }
                }

                QString saveError;
                if (!FileOps::writeFileAtomically(path, newData, &saveError)) {
                    QMessageBox::warning(this, tr("Save"),
                                         saveError.isEmpty() ? tr("Could not save %1.").arg(name)
                                                             : saveError);
                    content = editor->toPlainText();
                    continue;
                }
                saved = true;
                continue;
            }

            const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                + QStringLiteral("/lister-edit");
            QDir().mkpath(tempDir);
            const QString savePath = QDir(tempDir).filePath(name);
            QFile f(savePath);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)
                || f.write(newData) != newData.size()) {
                f.close();
                QFile::remove(savePath);
                QMessageBox::warning(this, tr("Save"),
                                     tr("Could not prepare the edited copy of %1.").arg(name));
                content = editor->toPlainText();
                continue;
            }
            f.close();

            QString saveError;
            const bool uploaded
                = Vfs::instance().put(VfsUri::parentUri(uri), name, savePath, &saveError);
            QFile::remove(savePath);
            if (!uploaded) {
                QMessageBox::warning(this, tr("Save"),
                                     saveError.isEmpty() ? tr("Could not save %1.").arg(name)
                                                         : saveError);
                content = editor->toPlainText();
                continue;
            }
            saved = true;
        }
    }
}

void MainWindow::findFiles()
{
    Panel *panel = activePanel();
    const bool localRoot = Vfs::isLocal(panel->uri());

    QDialog criteria(this);
    criteria.setWindowTitle(tr("Find files"));
    auto *form = new QFormLayout(&criteria);
    auto *nameEdit = new QLineEdit(&criteria);
    auto *extEdit = new QLineEdit(&criteria);
    auto *minSize = new QLineEdit(&criteria);
    auto *maxSize = new QLineEdit(&criteria);
    auto *daysEdit = new QLineEdit(&criteria);
    auto *dirsCheck = new QCheckBox(tr("Include folders"), &criteria);
    dirsCheck->setChecked(false);
    auto *contentCheck = new QCheckBox(tr("Search inside text files"), &criteria);
    contentCheck->setChecked(false);
    contentCheck->setEnabled(localRoot);
    contentCheck->setToolTip(localRoot
        ? tr("Also match text files whose contents contain the text above. Binary files "
             "are not read.")
        : tr("Content search reads local files only. This search matches names, sizes "
             "and dates in the archive or remote folder."));

    // Saved search criteria: store the current set of filters under a name and
    // recall it later, so a search that is run often does not have to be
    // retyped.
    auto *savedRow = new QWidget(&criteria);
    auto *savedLayout = new QHBoxLayout(savedRow);
    savedLayout->setContentsMargins(0, 0, 0, 0);
    auto *savedCombo = new QComboBox(savedRow);
    savedCombo->setEditable(false);
    savedCombo->setMinimumWidth(160);
    auto *saveButton = new QPushButton(tr("Save…"), savedRow);
    auto *deleteButton = new QPushButton(tr("Delete"), savedRow);
    savedLayout->addWidget(savedCombo, 1);
    savedLayout->addWidget(saveButton);
    savedLayout->addWidget(deleteButton);

    const auto loadSavedList = [&]() {
        QSettings settings;
        const QVariantList list = settings.value(QStringLiteral("search/saved")).toList();
        savedCombo->clear();
        savedCombo->addItem(tr("(none)"));
        for (const QVariant &entry : list) {
            const QVariantMap map = entry.toMap();
            savedCombo->addItem(map.value(QStringLiteral("name")).toString(), map);
        }
    };
    const auto applySaved = [&](const QVariantMap &map) {
        if (map.isEmpty())
            return;
        nameEdit->setText(map.value(QStringLiteral("nameContains")).toString());
        extEdit->setText(map.value(QStringLiteral("extension")).toString());
        minSize->setText(map.value(QStringLiteral("minSize")).toString());
        maxSize->setText(map.value(QStringLiteral("maxSize")).toString());
        daysEdit->setText(map.value(QStringLiteral("days")).toString());
        dirsCheck->setChecked(map.value(QStringLiteral("includeDirs")).toBool());
        contentCheck->setChecked(map.value(QStringLiteral("searchContents")).toBool());
    };
    loadSavedList();
    connect(savedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &criteria,
            [&, applySaved](int index) {
                if (index <= 0)
                    return;
                const QVariantMap map = savedCombo->itemData(index).toMap();
                applySaved(map);
            });
    connect(saveButton, &QPushButton::clicked, &criteria, [&]() {
        bool accepted = false;
        const QString name = QInputDialog::getText(&criteria, tr("Save search"),
                                                   tr("Name:"), QLineEdit::Normal,
                                                   QString(), &accepted);
        if (!accepted || name.trimmed().isEmpty())
            return;
        QVariantMap map;
        map.insert(QStringLiteral("name"), name.trimmed());
        map.insert(QStringLiteral("nameContains"), nameEdit->text().trimmed());
        map.insert(QStringLiteral("extension"), extEdit->text().trimmed());
        map.insert(QStringLiteral("minSize"), minSize->text().trimmed());
        map.insert(QStringLiteral("maxSize"), maxSize->text().trimmed());
        map.insert(QStringLiteral("days"), daysEdit->text().trimmed());
        map.insert(QStringLiteral("includeDirs"), dirsCheck->isChecked());
        map.insert(QStringLiteral("searchContents"), contentCheck->isChecked());

        QSettings settings;
        QVariantList list = settings.value(QStringLiteral("search/saved")).toList();
        // Replace an existing entry with the same name instead of duplicating.
        for (int i = 0; i < list.size();) {
            if (list.at(i).toMap().value(QStringLiteral("name")).toString() == name.trimmed())
                list.removeAt(i);
            else
                ++i;
        }
        list.append(map);
        settings.setValue(QStringLiteral("search/saved"), list);
        loadSavedList();
        const int idx = savedCombo->findText(name.trimmed());
        if (idx >= 0)
            savedCombo->setCurrentIndex(idx);
    });
    connect(deleteButton, &QPushButton::clicked, &criteria, [&]() {
        const int index = savedCombo->currentIndex();
        if (index <= 0)
            return;
        const QString name = savedCombo->currentText();
        QSettings settings;
        QVariantList list = settings.value(QStringLiteral("search/saved")).toList();
        for (int i = 0; i < list.size();) {
            if (list.at(i).toMap().value(QStringLiteral("name")).toString() == name)
                list.removeAt(i);
            else
                ++i;
        }
        settings.setValue(QStringLiteral("search/saved"), list);
        loadSavedList();
    });

    form->addRow(tr("Saved"), savedRow);
    form->addRow(tr("Name contains"), nameEdit);
    form->addRow(tr("Extension"), extEdit);
    form->addRow(tr("Min size (KB)"), minSize);
    form->addRow(tr("Max size (KB)"), maxSize);
    form->addRow(tr("Modified within (days)"), daysEdit);
    form->addRow(dirsCheck);
    form->addRow(contentCheck);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &criteria);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &criteria, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &criteria, &QDialog::reject);
    if (criteria.exec() != QDialog::Accepted)
        return;

    const QString needle = nameEdit->text().trimmed();
    const QString ext = extEdit->text().trimmed();
    const qint64 minBytes = minSize->text().trimmed().isEmpty() ? 0
        : minSize->text().trimmed().toLongLong() * 1024;
    const qint64 maxBytes = maxSize->text().trimmed().isEmpty() ? -1
        : maxSize->text().trimmed().toLongLong() * 1024;
    const int days = daysEdit->text().trimmed().isEmpty() ? -1 : daysEdit->text().trimmed().toInt();
    const QDateTime cutoff = days > 0 ? QDateTime::currentDateTime().addDays(-days) : QDateTime();

    const QString root = localRoot ? Vfs::localPathOf(panel->uri()) : panel->uri();

    SearchScan::Query query;
    query.nameContains = needle;
    query.extension = ext;
    query.minBytes = minBytes;
    query.maxBytes = maxBytes;
    query.modifiedAfter = cutoff;
    query.includeFolders = dirsCheck->isChecked();
    query.searchContents = localRoot && contentCheck->isChecked();
    query.maximumResults = 5000;

    QProgressDialog progress(tr("Searching…"), tr("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(300);

    // The scan runs on a worker thread: painting, scrolling and the Cancel
    // button keep answering however large the folder is.
    // A local match is an absolute path; a VFS match is a full entry URI.
    const auto describe = [localRoot, root](const QString &uri) {
        if (localRoot)
            return QDir(root).relativeFilePath(uri);
        QString relative = uri;
        if (relative.startsWith(root))
            relative.remove(0, root.size());
        while (relative.startsWith(QLatin1Char('!')) || relative.startsWith(QLatin1Char('/')))
            relative.remove(0, 1);
        return relative.isEmpty() ? vfsLeafName(uri) : relative;
    };

    SearchScan::Result result;
    QEventLoop loop;
    SearchTask *search = localRoot ? SearchTask::start(root, query, this)
                                   : SearchTask::startVfs(root, query, this);
    connect(search, &SearchTask::progress, &progress,
            [&progress, describe](int scanned, int found, const QString &current) {
                progress.setLabelText(tr("%1 entries scanned, %2 matches\n%3")
                                          .arg(scanned)
                                          .arg(found)
                                          .arg(describe(current)));
            });
    connect(&progress, &QProgressDialog::canceled, search, &SearchTask::cancel);
    connect(search, &SearchTask::finished, &loop,
            [&result, &loop](const SearchScan::Result &finished) {
                result = finished;
                loop.quit();
            });
    progress.show();
    loop.exec();
    progress.close();

    const QStringList matches = result.matches;
    const QString resultText = SearchScan::summary(result);

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Find results"));
    dialog.resize(680, 440);
    auto *layout = new QVBoxLayout(&dialog);
    auto *list = new QListWidget(&dialog);
    for (const QString &match : matches) {
        auto *item = new QListWidgetItem(describe(match), list);
        item->setData(Qt::UserRole, match);
    }
    auto *summary = new QLabel(resultText, &dialog);
    summary->setWordWrap(true);
    layout->addWidget(summary);
    layout->addWidget(list, 1);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(btns);
    connect(btns, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(list, &QListWidget::itemActivated, &dialog, [&dialog](QListWidgetItem *item) {
        if (item)
            dialog.setProperty("selected", item->data(Qt::UserRole).toString());
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString selected = dialog.property("selected").toString();
    if (selected.isEmpty())
        return;

    // parentUri and leaf names work for local paths, archive members and remote
    // entries alike, so a result can be opened wherever the search ran.
    panel->setUri(VfsUri::parentUri(selected));
    panel->selectName(vfsLeafName(selected));
    panel->focusLister();
}

void MainWindow::compareFolders()
{
    Panel *left = m_source;
    Panel *right = m_dest;
    if (!Vfs::isLocal(left->uri()) || !Vfs::isLocal(right->uri())) {
        QMessageBox::information(this, tr("Compare"),
                                 tr("Compare works in local folders."));
        return;
    }

    const QString leftRoot = Vfs::localPathOf(left->uri());
    const QString rightRoot = Vfs::localPathOf(right->uri());

    QDialog options(this);
    options.setWindowTitle(tr("Compare folders"));
    auto *layout = new QVBoxLayout(&options);
    auto *intro = new QLabel(tr("Compare the two local folders recursively."), &options);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *leftLabel = new QLabel(tr("<b>Left:</b> %1").arg(QDir::toNativeSeparators(leftRoot)),
                                 &options);
    leftLabel->setWordWrap(true);
    layout->addWidget(leftLabel);
    auto *rightLabel = new QLabel(tr("<b>Right:</b> %1").arg(QDir::toNativeSeparators(rightRoot)),
                                  &options);
    rightLabel->setWordWrap(true);
    layout->addWidget(rightLabel);

    auto *contents = new QCheckBox(tr("Compare file contents (SHA-256)"), &options);
    contents->setToolTip(tr("Read both files when their sizes match. This can take time in large folders."));
    layout->addWidget(contents);
    auto *hidden = new QCheckBox(tr("Include hidden files"), &options);
    hidden->setChecked(true);
    layout->addWidget(hidden);

    auto *extLabel = new QLabel(tr("Extensions (comma-separated, blank = all):"), &options);
    layout->addWidget(extLabel);
    auto *extEdit = new QLineEdit(&options);
    extEdit->setPlaceholderText(tr("e.g. cpp,h,txt"));
    layout->addWidget(extEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &options);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Compare"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &options, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &options, &QDialog::reject);
    if (options.exec() != QDialog::Accepted)
        return;

    FolderCompare::Options compareOptions;
    compareOptions.includeHidden = hidden->isChecked();
    compareOptions.compareContents = contents->isChecked();
    const QStringList extParts = extEdit->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : extParts)
        compareOptions.extensions.append(part.trimmed());

    auto cancel = std::make_shared<std::atomic_bool>(false);
    QProgressDialog progress(tr("Comparing folders…"), tr("Cancel"), 0, 0, this);
    progress.setWindowTitle(tr("Compare folders"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setWindowFlags(progress.windowFlags() & ~Qt::WindowCloseButtonHint);

    QFutureWatcher<FolderCompare::Result> watcher;
    QEventLoop loop;
    connect(&progress, &QProgressDialog::canceled, &loop, [cancel, &loop]() {
        cancel->store(true);
        loop.quit();
    });
    connect(&watcher, &QFutureWatcher<FolderCompare::Result>::finished, &loop,
            &QEventLoop::quit);

    watcher.setFuture(QtConcurrent::run([leftRoot, rightRoot, compareOptions, cancel]() {
        return FolderCompare::run(leftRoot, rightRoot, compareOptions, cancel.get());
    }));
    progress.show();

    // A fast or empty tree can finish before exec() starts. quit() on a loop
    // that is not running has no effect, so check the completion state from a
    // queued callback inside the loop as well as from the finished signal.
    QTimer::singleShot(0, &loop, [&watcher, cancel, &loop]() {
        if (watcher.isFinished() || cancel->load())
            loop.quit();
    });
    loop.exec();
    progress.close();
    watcher.waitForFinished();

    const FolderCompare::Result result = watcher.result();
    if (result.cancelled) {
        statusBar()->showMessage(tr("Folder comparison cancelled."), 4000);
        return;
    }
    if (!result.error.isEmpty()) {
        QMessageBox::warning(this, tr("Compare"), result.error);
        return;
    }

    QStringList onlyLeft;
    QStringList onlyRight;
    QStringList typeChanged;
    QStringList sizeChanged;
    QStringList contentChanged;
    QStringList linkChanged;
    QStringList readErrors;
    for (const FolderCompare::Item &item : result.items) {
        switch (item.kind) {
        case FolderCompare::Difference::OnlyLeft:
            onlyLeft.append(item.relativePath);
            break;
        case FolderCompare::Difference::OnlyRight:
            onlyRight.append(item.relativePath);
            break;
        case FolderCompare::Difference::TypeChanged:
            typeChanged.append(item.relativePath);
            break;
        case FolderCompare::Difference::SizeChanged:
            sizeChanged.append(tr("%1 (%2 → %3)")
                                   .arg(item.relativePath, humanSize(item.leftSize),
                                        humanSize(item.rightSize)));
            break;
        case FolderCompare::Difference::ContentChanged:
            contentChanged.append(item.relativePath);
            break;
        case FolderCompare::Difference::LinkChanged:
            linkChanged.append(tr("%1 (%2)").arg(item.relativePath, item.detail));
            break;
        case FolderCompare::Difference::ReadError:
            readErrors.append(tr("%1 (%2)").arg(item.relativePath, item.detail));
            break;
        }
    }

    QString out;
    out += tr("Left:  %1\nRight: %2\n\n").arg(QDir::toNativeSeparators(leftRoot),
                                               QDir::toNativeSeparators(rightRoot));
    out += tr("Compared %1 files and %2 directories.\n")
               .arg(result.filesCompared)
               .arg(result.directoriesCompared);
    if (compareOptions.compareContents)
        out += tr("Hashed %1.\n").arg(humanSize(result.bytesHashed));
    else
        out += tr("Content comparison was off; same-size files were treated as equal.\n");
    out += QLatin1Char('\n');

    const auto appendSection = [&out](const QString &title, const QStringList &lines) {
        if (lines.isEmpty())
            return;
        out += title + QStringLiteral(" (%1):\n").arg(lines.size());
        for (const QString &line : lines)
            out += QStringLiteral("  %1\n").arg(line);
        out += QLatin1Char('\n');
    };

    if (result.incomplete) {
        out += tr("The comparison is incomplete: part of the tree could not be read, "
                   "so this is not a clean match.\n");
    }
    if (result.items.isEmpty()) {
        out += result.incomplete ? tr("No differences were found in the parts that could "
                                      "be read.\n")
                                 : tr("The folders match.\n");
    } else {
        appendSection(tr("Only in left"), onlyLeft);
        appendSection(tr("Only in right"), onlyRight);
        appendSection(tr("Type differs"), typeChanged);
        appendSection(tr("Size differs"), sizeChanged);
        appendSection(tr("Content differs"), contentChanged);
        appendSection(tr("Link target differs"), linkChanged);
        appendSection(tr("Could not read"), readErrors);
    }
    appendSection(tr("Not readable"), result.unreadable);

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Folder compare"));
    dialog.resize(820, 560);
    auto *resultLayout = new QVBoxLayout(&dialog);
    auto *text = new QPlainTextEdit(&dialog);
    text->setReadOnly(true);
    text->setLineWrapMode(QPlainTextEdit::NoWrap);
    text->setPlainText(out);
    resultLayout->addWidget(text, 1);
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    resultLayout->addWidget(close);
    connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

void MainWindow::refreshBoth()
{
    m_source->model()->refresh();
    m_dest->model()->refresh();
}

void MainWindow::registerCommands()
{
    CommandRegistry &registry = CommandRegistry::instance();

    registry.add({QStringLiteral("lister.open"), tr("Open"), tr("Navigation"),
                  [this](CommandContext &) { navigateInto(activePanel()); }});

    registry.add({QStringLiteral("lister.parent"), tr("Go up one level"), tr("Navigation"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      panel->setUri(VfsUri::parentUri(panel->uri()));
                      panel->focusLister();
                  }});

    registry.add({QStringLiteral("lister.home"), tr("Home"), tr("Navigation"),
                  [this](CommandContext &) {
                      activePanel()->setUri(QUrl::fromLocalFile(QDir::homePath()).toString());
                  }});

    registry.add({QStringLiteral("lister.refresh"), tr("Refresh"), tr("View"),
                  [this](CommandContext &) { refreshBoth(); }});

    registry.add({QStringLiteral("lister.focusPath"), tr("Go to address"), tr("Navigation"),
                  [this](CommandContext &) { activePanel()->focusPathBar(); }});

    registry.add({QStringLiteral("lister.historyBack"), tr("Back"), tr("Navigation"),
                  [this](CommandContext &) {
                      activePanel()->goBack();
                      activePanel()->focusLister();
                  }});

    registry.add({QStringLiteral("lister.historyForward"), tr("Forward"), tr("Navigation"),
                  [this](CommandContext &) {
                      activePanel()->goForward();
                      activePanel()->focusLister();
                  }});

    registry.add({QStringLiteral("go.recent"), tr("Recent folders…"), tr("Navigation"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      const QStringList uris = panel->recentFolders();
                      if (uris.isEmpty()) {
                          QMessageBox::information(this, tr("Recent folders"),
                                                  tr("No previous folders in this panel yet."));
                          return;
                      }
                      QStringList labels;
                      for (const QString &uri : uris)
                          labels.append(Panel::displayFor(uri));
                      bool ok = false;
                      const QString label = QInputDialog::getItem(
                          this, tr("Recent folders"), tr("Folder:"), labels, 0, false, &ok);
                      if (!ok || label.isEmpty())
                          return;
                      const int index = labels.indexOf(label);
                      if (index < 0)
                          return;
                      panel->setUri(uris.at(index));
                      panel->focusLister();
                  }});

    registry.add({QStringLiteral("view.filter"), tr("Filter this folder"), tr("View"),
                  [this](CommandContext &) { activePanel()->focusFilterBar(); }});

    registry.add({QStringLiteral("view.icons"), tr("Icon view"), tr("View"),
                  [this](CommandContext &) {
                      const bool icons = !m_source->iconView();
                      m_source->setIconView(icons);
                      m_dest->setIconView(icons);
                      QSettings settings;
                      settings.setValue(QStringLiteral("ui/iconView"), icons);
                      settings.setValue(QStringLiteral("view/iconView/") + m_source->uri(), icons);
                      settings.setValue(QStringLiteral("view/iconView/") + m_dest->uri(), icons);
                      statusBar()->showMessage(icons ? tr("Icon view.") : tr("Details view."),
                                               2500);
                  }});

    registry.add({QStringLiteral("lister.toggleHidden"), tr("Show hidden"), tr("View"),
                  [this](CommandContext &) {
                      const bool show = !activePanel()->model()->showHidden();
                      m_source->model()->setShowHidden(show);
                      m_dest->model()->setShowHidden(show);
                  }});

    registry.add({QStringLiteral("selection.toggle"), tr("Select and move down"), tr("Selection"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      ListerView *view = panel->view();
                      QItemSelectionModel *selection = view->selectionModel();
                      const QModelIndex current = view->currentIndex();
                      if (!current.isValid())
                          return;

                      selection->select(current,
                                        selection->isSelected(current)
                                            ? QItemSelectionModel::Deselect
                                            : QItemSelectionModel::Select);

                      const QModelIndex next = view->model()->index(current.row() + 1, 0);
                      if (next.isValid())
                          view->setCurrentIndex(next);
                  }});

    registry.add({QStringLiteral("selection.sameType"), tr("Select files of same type"), tr("Selection"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      ListerView *view = panel->view();
                      DirModel *model = panel->model();
                      const QModelIndex current = view->currentIndex();
                      int anchorRow = current.isValid() ? current.row() : -1;
                      if (anchorRow < 0) {
                          const QModelIndexList sel = view->selectionModel()->selectedIndexes();
                          if (!sel.isEmpty())
                              anchorRow = sel.first().row();
                      }
                      if (anchorRow < 0 || anchorRow >= model->rowCount())
                          return;

                      const QString anchorName = model->entry(anchorRow).name;
                      if (model->entry(anchorRow).isDir)
                          return;
                      const QString suffix = QFileInfo(anchorName).suffix().toLower();

                      // Clear the current selection, then select every file whose
                      // extension matches the anchor. Directories are never
                      // swept in, even when they share the suffix.
                      view->selectionModel()->clearSelection();
                      for (int row = 0; row < model->rowCount(); ++row) {
                          const Entry &entry = model->entry(row);
                          if (entry.isDir)
                              continue;
                          if (QFileInfo(entry.name).suffix().toLower() != suffix)
                              continue;
                          const QModelIndex index = model->index(row, 0);
                          view->selectionModel()->select(index, QItemSelectionModel::Select);
                      }
                      if (current.isValid())
                          view->setCurrentIndex(current);
                  }});

    registry.add({QStringLiteral("selection.byPattern"), tr("Select by pattern…"), tr("Selection"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      ListerView *view = panel->view();
                      DirModel *model = panel->model();
                      if (model->rowCount() == 0)
                          return;

                      QDialog dialog(this);
                      dialog.setWindowTitle(tr("Select by pattern"));
                      auto *layout = new QVBoxLayout(&dialog);
                      auto *patternEdit = new QLineEdit(&dialog);
                      patternEdit->setPlaceholderText(tr("*.png, report*, ??.txt"));
                      auto *countLabel = new QLabel(&dialog);
                      layout->addWidget(new QLabel(tr("Pattern (wildcards * and ?):"), &dialog));
                      layout->addWidget(patternEdit);
                      layout->addWidget(countLabel);
                      auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                      layout->addWidget(buttons);
                      connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                      connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

                      // Listi matches visible names case-sensitively; NoPatternOption
                      // makes that explicit rather than relying on the default.
                      const auto compilePattern = [](const QString &pattern) {
                          return QRegularExpression(
                              QRegularExpression::wildcardToRegularExpression(pattern),
                              QRegularExpression::NoPatternOption);
                      };
                      const auto updateCount = [&]() {
                          const QString pattern = patternEdit->text();
                          if (pattern.isEmpty()) {
                              countLabel->setText(tr("0 matching names"));
                              return;
                          }
                          const QRegularExpression regex = compilePattern(pattern);
                          int matches = 0;
                          for (int row = 0; row < model->rowCount(); ++row) {
                              const QString name = model->data(model->index(row, 0), Qt::DisplayRole).toString();
                              if (regex.match(name).hasMatch())
                                  ++matches;
                          }
                          countLabel->setText(tr("%1 matching names").arg(matches));
                      };
                      connect(patternEdit, &QLineEdit::textChanged, &dialog, updateCount);
                      updateCount();

                      if (dialog.exec() != QDialog::Accepted)
                          return;
                      const QString pattern = patternEdit->text().trimmed();
                      if (pattern.isEmpty())
                          return;
                      const QRegularExpression regex = compilePattern(pattern);

                      view->selectionModel()->clearSelection();
                      for (int row = 0; row < model->rowCount(); ++row) {
                          const QString name = model->data(model->index(row, 0), Qt::DisplayRole).toString();
                          if (!regex.match(name).hasMatch())
                              continue;
                          view->selectionModel()->select(model->index(row, 0), QItemSelectionModel::Select);
                      }
                      if (view->currentIndex().isValid())
                          view->setCurrentIndex(view->currentIndex());
                  }});

    registry.add({QStringLiteral("file.copy"), tr("Copy here"), tr("Files"),
                  [this](CommandContext &) { copyToOtherPanel(false); }});

    registry.add({QStringLiteral("file.move"), tr("Move here"), tr("Files"),
                  [this](CommandContext &) { copyToOtherPanel(true); }});

        registry.add({QStringLiteral("file.mkdir"), tr("New folder"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      if (!Vfs::instance().writable(panel->uri())) {
                          QMessageBox::information(this, tr("Read-only"),
                                                  tr("This location is read-only."));
                          return;
                      }

                      bool accepted = false;
                      const QString name = QInputDialog::getText(
                          this, tr("New folder"), tr("Name:"), QLineEdit::Normal, tr("New folder"),
                          &accepted);
                      if (!accepted || name.trimmed().isEmpty())
                          return;
                      if (!PathSafety::isSafePathComponent(name.trimmed())) {
                          QMessageBox::warning(this, tr("New folder"),
                                              tr("That name contains unsafe characters."));
                          return;
                      }

                      QString error;
                      if (Vfs::isLocal(panel->uri())) {
                          QString created;
                          if (!FileOps::makeDirectory(Vfs::localPathOf(panel->uri()), name.trimmed(),
                                                      &created, &error)) {
                              QMessageBox::warning(this, tr("New folder"), error);
                              return;
                          }
                          UndoStack::Entry ue;
                          ue.type = UndoStack::OpType::Mkdir;
                          ue.destinations = {created};
                          m_undoStack.push(ue);
                          panel->model()->refresh();
                          panel->selectName(QFileInfo(created).fileName());
                          return;
                      }

                      if (!Vfs::instance().mkdir(panel->uri(), name.trimmed(), &error)) {
                          QMessageBox::warning(this, tr("New folder"), error);
                          return;
                      }
                      UndoStack::Entry ue;
                      ue.type = UndoStack::OpType::Mkdir;
                      ue.destinations = {VfsUri::childUri(panel->uri(), name.trimmed())};
                      m_undoStack.push(ue);
                      panel->model()->refresh();
                      panel->selectName(name.trimmed());
                  }});

    registry.add({QStringLiteral("file.newFile"), tr("New file"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      if (!Vfs::isLocal(panel->uri())) {
                          QMessageBox::information(this, tr("New file"),
                                                  tr("New files can be created in local folders."));
                          return;
                      }
                      if (!Vfs::instance().writable(panel->uri())) {
                          QMessageBox::information(this, tr("Read-only"),
                                                  tr("This location is read-only."));
                          return;
                      }

                      bool accepted = false;
                      const QString name = QInputDialog::getText(
                          this, tr("New file"), tr("Name:"), QLineEdit::Normal,
                          tr("New file"), &accepted);
                      if (!accepted || name.trimmed().isEmpty())
                          return;

                      QString created;
                      QString error;
                      if (!FileOps::createFile(Vfs::localPathOf(panel->uri()), name.trimmed(),
                                               &created, &error)) {
                          QMessageBox::warning(this, tr("New file"), error);
                          return;
                      }
                      UndoStack::Entry ue;
                      ue.type = UndoStack::OpType::Copy; // undo removes the new file
                      ue.destinations = {created};
                      m_undoStack.push(ue);
                      panel->model()->refresh();
                      panel->selectName(QFileInfo(created).fileName());
                  }});

    registry.add({QStringLiteral("file.rename"), tr("Rename"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      if (!Vfs::instance().writable(panel->uri())) {
                          QMessageBox::information(this, tr("Read-only"),
                                                  tr("This location is read-only."));
                          return;
                      }

                      const QString current = panel->currentName();
                      if (current.isEmpty())
                          return;

                      bool accepted = false;
                      const QString name = QInputDialog::getText(this, tr("Rename"),
                                                                 tr("New name:"), QLineEdit::Normal,
                                                                 current, &accepted);
                      if (!accepted || name.trimmed().isEmpty() || name == current)
                          return;
                      if (!PathSafety::isSafePathComponent(name.trimmed())) {
                          QMessageBox::warning(this, tr("Rename"),
                                              tr("That name contains unsafe characters."));
                          return;
                      }

                      QString error;
                      if (Vfs::isLocal(panel->uri())) {
                          QString renamed;
                          if (!FileOps::rename(Vfs::localPathOf(panel->currentUri()), name.trimmed(),
                                               &renamed, &error)) {
                              QMessageBox::warning(this, tr("Rename"), error);
                              return;
                          }
                          UndoStack::Entry ue;
                          ue.type = UndoStack::OpType::Rename;
                          ue.sources = {Vfs::localPathOf(panel->currentUri())};
                          ue.destinations = {renamed};
                          m_undoStack.push(ue);
                          panel->model()->refresh();
                          panel->selectName(QFileInfo(renamed).fileName());
                          return;
                      }

                      QString newUri;
                      if (!Vfs::instance().rename(panel->currentUri(), name.trimmed(), &newUri, &error)) {
                          QMessageBox::warning(this, tr("Rename"), error);
                          return;
                      }
                      UndoStack::Entry ue;
                      ue.type = UndoStack::OpType::Rename;
                      ue.sources = {panel->currentUri()};
                      ue.destinations = {newUri};
                      m_undoStack.push(ue);
                      panel->model()->refresh();
                      panel->selectName(name.trimmed());
                  }});

    registry.add({QStringLiteral("file.renameBatch"), tr("Rename selected…"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      if (!Vfs::instance().writable(panel->uri())) {
                          QMessageBox::information(this, tr("Read-only"),
                                                  tr("This location is read-only."));
                          return;
                      }

                      const QStringList uris = panel->selectedUris();
                      if (uris.isEmpty()) {
                          statusBar()->showMessage(tr("Select something first."), 4000);
                          return;
                      }

                      QStringList names;
                      for (const QString &uri : uris)
                          names.append(vfsLeafName(uri));

                      QDialog dialog(this);
                      dialog.setWindowTitle(tr("Rename selected"));
                      dialog.resize(720, 480);
                      auto *layout = new QVBoxLayout(&dialog);

                      auto *form = new QFormLayout();
                      auto *findEdit = new QLineEdit(&dialog);
                      auto *replaceEdit = new QLineEdit(&dialog);
                      auto *prefixEdit = new QLineEdit(&dialog);
                      auto *suffixEdit = new QLineEdit(&dialog);
                      auto *keepExt = new QCheckBox(tr("Keep extension"), &dialog);
                      keepExt->setChecked(true);
                      auto *numbering = new QCheckBox(tr("Add numbering"), &dialog);
                      auto *startSpin = new QSpinBox(&dialog);
                      startSpin->setRange(1, 999999);
                      startSpin->setValue(1);
                      auto *startRow = new QWidget(&dialog);
                      auto *startLayout = new QHBoxLayout(startRow);
                      startLayout->setContentsMargins(0, 0, 0, 0);
                      startLayout->addWidget(numbering);
                      startLayout->addWidget(new QLabel(tr("start:"), &dialog));
                      startLayout->addWidget(startSpin);
                      startLayout->addStretch();

                      form->addRow(tr("Find"), findEdit);
                      form->addRow(tr("Replace with"), replaceEdit);
                      form->addRow(tr("Prefix"), prefixEdit);
                      form->addRow(tr("Suffix"), suffixEdit);
                      form->addRow(QString(), keepExt);
                      form->addRow(QString(), startRow);
                      layout->addLayout(form);

                      auto *preview = new QTableWidget(uris.size(), 2, &dialog);
                      preview->setHorizontalHeaderLabels({tr("Current name"), tr("New name")});
                      preview->horizontalHeader()->setStretchLastSection(true);
                      preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
                      for (int i = 0; i < names.size(); ++i)
                          preview->setItem(i, 0, new QTableWidgetItem(names.at(i)));
                      layout->addWidget(preview, 1);

                      auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                      buttons->button(QDialogButtonBox::Ok)->setText(tr("Rename"));
                      layout->addWidget(buttons);
                      connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                      connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

                      auto currentOptions = [&]() {
                          RenameBatch::Options options;
                          options.find = findEdit->text();
                          options.replace = replaceEdit->text();
                          options.prefix = prefixEdit->text();
                          options.suffix = suffixEdit->text();
                          options.keepExtension = keepExt->isChecked();
                          options.numbering = numbering->isChecked();
                          options.startNumber = startSpin->value();
                          return options;
                      };

                      auto updatePreview = [&]() {
                          const RenameBatch::Plan plan
                              = RenameBatch::plan(names, currentOptions());
                          for (int i = 0; i < plan.newNames.size(); ++i) {
                              // Mark duplicates for the preview only; the real
                              // rename still reports the filesystem's error.
                              QString display = plan.newNames.at(i);
                              if (plan.duplicate.at(i))
                                  display += tr("  (duplicate)");
                              if (plan.invalid.at(i))
                                  display += tr("  (invalid name)");
                              auto *item = preview->item(i, 1);
                              if (!item) {
                                  item = new QTableWidgetItem(display);
                                  preview->setItem(i, 1, item);
                              } else {
                                  item->setText(display);
                              }
                          }
                      };

                      connect(findEdit, &QLineEdit::textChanged, &dialog, updatePreview);
                      connect(replaceEdit, &QLineEdit::textChanged, &dialog, updatePreview);
                      connect(prefixEdit, &QLineEdit::textChanged, &dialog, updatePreview);
                      connect(suffixEdit, &QLineEdit::textChanged, &dialog, updatePreview);
                      connect(keepExt, &QCheckBox::toggled, &dialog, updatePreview);
                      connect(numbering, &QCheckBox::toggled, &dialog, updatePreview);
                      connect(startSpin, QOverload<int>::of(&QSpinBox::valueChanged), &dialog, updatePreview);
                      updatePreview();

                      if (dialog.exec() != QDialog::Accepted)
                          return;

                      // Rebuild the plan from the current fields instead of
                      // parsing the preview text, so a file whose name contains
                      // the duplicate marker is still renamed literally.
                      const RenameBatch::Plan plan
                          = RenameBatch::plan(names, currentOptions());
                      int failed = 0;
                      QStringList errors;
                      for (int i = 0; i < uris.size(); ++i) {
                          if (plan.unchanged.at(i))
                              continue;
                          if (plan.invalid.at(i)) {
                              failed++;
                              errors.append(tr("Unsafe name: %1").arg(plan.newNames.at(i)));
                              continue;
                          }
                          QString newUri;
                          QString error;
                          if (!Vfs::instance().rename(uris.at(i), plan.newNames.at(i),
                                                      &newUri, &error)) {
                              failed++;
                              errors.append(error);
                          }
                      }
                      panel->model()->refresh();
                      if (failed > 0)
                          QMessageBox::warning(this, tr("Rename"),
                                              tr("%1 of %2 renames failed.\n%3")
                                                  .arg(failed).arg(plan.renameCount())
                                                  .arg(errors.join(QLatin1Char('\n'))));
                      else
                          statusBar()->showMessage(tr("Renamed %1 items.").arg(plan.renameCount()),
                                                   4000);
                  }});

    registry.add({QStringLiteral("file.trash"), tr("Move to trash"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      if (!Vfs::instance().writable(panel->uri())) {
                          QMessageBox::information(this, tr("Read-only"),
                                                  tr("This location is read-only."));
                          return;
                      }

                      const QStringList uris = panel->selectedUris();
                      if (uris.isEmpty())
                          return;

                      if (!Vfs::isLocal(panel->uri())) {
                          const auto answer = QMessageBox::question(
                              this, tr("Delete"),
                              tr("Delete %1 permanently from this location?")
                                  .arg(itemCount(int(uris.size()))),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                          if (answer != QMessageBox::Yes)
                              return;

                          startTask(tr("Deleting %1").arg(itemCount(int(uris.size()))),
                                    [uris](FileOps::Progress &progress, QString *deleteError) {
                                        QStringList failed;
                                        for (const QString &uri : uris) {
                                            if (progress.cancelled && progress.cancelled())
                                                break;
                                            if (progress.current)
                                                progress.current(vfsLeafName(uri));
                                            QString error;
                                            if (!Vfs::instance().remove(uri, true, &error)) {
                                                if (deleteError)
                                                    *deleteError = error;
                                                failed.append(uri);
                                            }
                                            if (progress.itemDone)
                                                progress.itemDone();
                                        }
                                        return failed;
                                    });
                          return;
                      }

                      QStringList paths;
                      for (const QString &uri : uris)
                          paths.append(Vfs::localPathOf(uri));

                      // Each file is reported and undone on its own: a failure
                      // on the second file must not hide that the first one
                      // reached the trash, nor offer to delete it for good.
                      const FileOps::TrashResult trash = FileOps::moveToTrashDetailed(paths);
                      panel->model()->refresh();

                      if (!trash.trashed.isEmpty()) {
                          UndoStack::Entry ue;
                          ue.type = UndoStack::OpType::Trash;
                          ue.sources = trash.trashed;
                          ue.trashNames = trash.names;
                          m_undoStack.push(ue);
                      }

                      if (trash.failed.isEmpty()) {
                          statusBar()->showMessage(
                              tr("Moved %1 to the trash.").arg(itemCount(int(trash.trashed.size()))),
                              4000);
                          return;
                      }

                      if (trash.trashed.isEmpty()) {
                          // Nothing reached the trash at all (some containers and
                          // network homes have none), so offer the honest choice,
                          // for the files that failed only.
                          const auto answer = QMessageBox::question(
                              this, tr("Trash"),
                              tr("The trash is not available here.\n\n%1\n\nDelete %2 "
                                 "permanently?")
                                  .arg(trash.errors.join(QLatin1Char('\n')),
                                       itemCount(int(trash.failed.size()))),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                          if (answer != QMessageBox::Yes)
                              return;

                          const QStringList doomed = trash.failed;
                          startTask(tr("Deleting %1").arg(itemCount(int(doomed.size()))),
                                    [doomed](FileOps::Progress &progress, QString *deleteError) {
                                        FileOps::removePermanently(doomed, deleteError, progress);
                                        return QStringList();
                                    });
                          return;
                      }

                      QMessageBox::warning(
                          this, tr("Trash"),
                          tr("%1 item(s) were moved to the trash, but %2 could not be:\n\n%3")
                              .arg(trash.trashed.size())
                              .arg(trash.failed.size())
                              .arg(trash.errors.join(QLatin1Char('\n'))));
                  }});

    registry.add({QStringLiteral("file.compress"), tr("Compress to archive…"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *from = activePanel();
                      Panel *to = inactivePanel();
                      Panel *target = Vfs::instance().writable(from->uri()) ? from : to;
                      if (!Vfs::instance().writable(target->uri())) {
                          QMessageBox::information(this, tr("Compress"),
                                                  tr("Choose a writable folder first."));
                          return;
                      }

                      const QModelIndexList selected
                          = from->view()->selectionModel()->selectedIndexes();
                      if (selected.isEmpty()) {
                          statusBar()->showMessage(tr("Select something first."), 4000);
                          return;
                      }

                      bool accepted = false;
                      const QString name = QInputDialog::getText(
                          this, tr("Compress to archive"), tr("Archive name:"), QLineEdit::Normal,
                          QStringLiteral("archive.zip"), &accepted);
                      if (!accepted || name.trimmed().isEmpty())
                          return;

                      bool pwdAccepted = false;
                      const QString password = QInputDialog::getText(
                          this, tr("Compress to archive"),
                          tr("Password (leave empty for no encryption):"), QLineEdit::Password,
                          QString(), &pwdAccepted);
                      if (!pwdAccepted)
                          return;

                      QVector<FileOps::CopyItem> items;
                      for (const QModelIndex &index : selected) {
                          FileOps::CopyItem item;
                          item.uri = index.data(DirModel::UriRole).toString();
                          item.isDir = index.data(DirModel::IsDirRole).toBool();
                          items.append(item);
                      }

                      QString archivePath = name.trimmed();
                      if (!archivePath.contains(QLatin1Char('.')))
                          archivePath += QStringLiteral(".zip");
                      if (!ArchivePaths::isWritablePath(archivePath)) {
                          QMessageBox::warning(this, tr("Compress"),
                                              tr("Listista cannot create archives in this format. "
                                                 "ARJ, CAB, LHA and RAR5 archives can be browsed "
                                                 "and extracted, but not created."));
                          return;
                      }
                      const QString fileUri = VfsUri::childUri(
                          target->uri(), archivePath);
                      if (!password.isEmpty()) {
                          const QString archiveUri = VfsUri::archiveUriFor(fileUri);
                          setArchivePassword(VfsUri::parse(archiveUri).path, password);
                      }
                      runTransfer(items, VfsUri::archiveUriFor(fileUri), false);
                  }});

    registry.add({QStringLiteral("file.extract"), tr("Extract here"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *from = activePanel();
                      Panel *target = Vfs::instance().writable(from->uri()) ? from : inactivePanel();
                      if (!Vfs::isLocal(target->uri())) {
                          QMessageBox::information(this, tr("Extract"),
                                                  tr("Choose a local folder to extract into."));
                          return;
                      }

                      QVector<FileOps::CopyItem> items;
                      for (const QString &uri : from->selectedUris()) {
                          if (!VfsUri::looksLikeArchive(uri))
                              continue;
                          const QString root = VfsUri::archiveUriFor(uri);
                          QVector<Entry> entries;
                          QString error;
                          if (!Vfs::instance().list(root, true, &entries, &error)) {
                              QMessageBox::warning(this, tr("Extract"), error);
                              continue;
                          }
                          for (const Entry &entry : entries) {
                              FileOps::CopyItem item;
                              item.uri = VfsUri::childUri(root, entry.name);
                              item.isDir = entry.isDir;
                              items.append(item);
                          }
                      }

                      if (items.isEmpty()) {
                          statusBar()->showMessage(tr("Select an archive first."), 4000);
                          return;
                      }
                      runTransfer(items, target->uri(), false);
                  }});

    registry.add({QStringLiteral("file.copyPath"), tr("Copy path"), tr("Files"),
                  [this](CommandContext &) {
                      QStringList uris = activePanel()->selectedUris();
                      if (uris.isEmpty())
                          uris.append(activePanel()->uri());
                      QStringList paths;
                      for (const QString &uri : uris)
                          paths.append(Panel::displayFor(uri));
                      QApplication::clipboard()->setText(paths.join(QLatin1Char('\n')));
                      statusBar()->showMessage(tr("Path copied."), 3000);
                  }});

    registry.add({QStringLiteral("file.exportList"), tr("Export file list…"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      DirModel *model = panel->model();
                      if (model->rowCount() == 0)
                          return;

                      const QModelIndexList selected
                          = panel->view()->selectionModel()->selectedIndexes();
                      QDialog dialog(this);
                      dialog.setWindowTitle(tr("Export file list"));
                      auto *layout = new QVBoxLayout(&dialog);
                      auto *selectedRadio = new QRadioButton(
                          tr("Selected entries (%1)").arg(selected.size()), &dialog);
                      selectedRadio->setEnabled(!selected.isEmpty());
                      auto *allRadio = new QRadioButton(
                          tr("All visible entries (%1)").arg(model->rowCount()), &dialog);
                      // A partial selection must start on "Selected entries"; an
                      // unchecked "Selected" radio used to mean "everything".
                      selectedRadio->setChecked(!selected.isEmpty());
                      allRadio->setChecked(selected.isEmpty());
                      auto *countLabel = new QLabel(&dialog);
                      auto *preview = new QPlainTextEdit(&dialog);
                      preview->setReadOnly(true);
                      preview->setMaximumHeight(90);
                      layout->addWidget(selectedRadio);
                      layout->addWidget(allRadio);
                      layout->addWidget(countLabel);
                      layout->addWidget(new QLabel(tr("First three rows:"), &dialog));
                      layout->addWidget(preview);
                      auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                      layout->addWidget(buttons);
                      connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                      connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

                      const auto buildRows = [&](bool onlySelected) {
                          QVector<FileListExport::Row> rows;
                          for (int row = 0; row < model->rowCount(); ++row) {
                              const QModelIndex index = model->index(row, 0);
                              if (onlySelected && !panel->view()->selectionModel()->isSelected(index))
                                  continue;
                              const QString uri = index.data(DirModel::UriRole).toString();
                              const bool isDir = index.data(DirModel::IsDirRole).toBool();
                              const QString localPath = Vfs::isLocal(uri) ? Vfs::localPathOf(uri) : QString();
                              const bool isLink = !localPath.isEmpty() && QFileInfo(localPath).isSymLink();

                              FileListExport::Row exportRow;
                              exportRow.name = index.data(Qt::DisplayRole).toString();
                              exportRow.fullPath = Panel::displayFor(uri);
                              exportRow.type = isLink ? QStringLiteral("link")
                                                     : (isDir ? QStringLiteral("folder")
                                                              : QStringLiteral("file"));
                              if (!isDir)
                                  exportRow.size = QString::number(
                                      index.data(DirModel::SizeBytesRole).toLongLong());
                              exportRow.modified = FileListExport::modifiedUtc(
                                  index.data(DirModel::ModifiedRole).toDateTime());
                              rows.append(exportRow);
                          }
                          return rows;
                      };
                      const auto updatePreview = [&]() {
                          const QVector<FileListExport::Row> rows
                              = buildRows(selectedRadio->isChecked());
                          countLabel->setText(tr("%1 entries will be exported.").arg(rows.size()));
                          QStringList sample;
                          for (int i = 0; i < rows.size() && i < 3; ++i)
                              sample << rows.at(i).name + QStringLiteral("  ·  ")
                                      + rows.at(i).fullPath;
                          preview->setPlainText(sample.join(QLatin1Char('\n')));
                      };
                      connect(selectedRadio, &QRadioButton::toggled, &dialog,
                              [&](bool) { if (selectedRadio->isChecked()) updatePreview(); });
                      connect(allRadio, &QRadioButton::toggled, &dialog,
                              [&](bool) { if (allRadio->isChecked()) updatePreview(); });
                      updatePreview();
                      if (dialog.exec() != QDialog::Accepted)
                          return;

                      const bool onlySelected = selectedRadio->isChecked();
                      const QVector<FileListExport::Row> rows = buildRows(onlySelected);
                      if (rows.isEmpty())
                          return;
                      const QString base = Vfs::isLocal(panel->uri())
                          ? Vfs::localPathOf(panel->uri()) : QDir::homePath();
                      QString path = QFileDialog::getSaveFileName(
                          this, tr("Export file list"),
                          QDir(base).filePath(QStringLiteral("file-list.csv")),
                          tr("CSV files (*.csv);;All files (*)"));
                      if (path.isEmpty())
                          return;
                      if (QFileInfo::exists(path)) {
                          QMessageBox::warning(this, tr("Export file list"),
                                              tr("The file already exists. Choose a new filename."));
                          return;
                      }
                      QString error;
                      if (!FileListExport::writeNew(path, rows, &error)) {
                          QMessageBox::warning(this, tr("Export file list"), error);
                          return;
                      }
                      statusBar()->showMessage(tr("Exported %1 entries to %2").arg(rows.size()).arg(path),
                                               5000);
                  }});

    registry.add({QStringLiteral("file.openWith"), tr("Open with…"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      const QStringList uris = panel->selectedUris();
                      if (uris.isEmpty())
                          return;
                      const QString app = QFileDialog::getOpenFileName(
                          this, tr("Choose application"), QStringLiteral("/usr/bin"));
                      if (app.isEmpty())
                          return;
                      QStringList args;
                      for (const QString &uri : uris) {
                          if (Vfs::isLocal(uri))
                              args.append(Vfs::localPathOf(uri));
                          else {
                              QString error;
                              const QString local = TemporaryCopy::ofUri(uri, &error);
                              if (!local.isEmpty())
                                  args.append(local);
                          }
                      }
                      QProcess::startDetached(app, args);
                  }});

    registry.add({QStringLiteral("file.view"), tr("View"), tr("Files"),
                  [this](CommandContext &) { viewFiles(false); }});

    registry.add({QStringLiteral("file.edit"), tr("Edit"), tr("Files"),
                  [this](CommandContext &) { viewFiles(true); }});

    registry.add({QStringLiteral("file.clipboardCopy"), tr("Copy to clipboard"), tr("Files"),
                  [this](CommandContext &) { clipboardCopy(false); }});

    registry.add({QStringLiteral("file.clipboardCut"), tr("Cut to clipboard"), tr("Files"),
                  [this](CommandContext &) { clipboardCopy(true); }});

    registry.add({QStringLiteral("file.clipboardPaste"), tr("Paste from clipboard"), tr("Files"),
                  [this](CommandContext &) { clipboardPaste(); }});

    registry.add({QStringLiteral("file.clipboardToFile"), tr("Save clipboard to file"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      const QString targetDir = Vfs::isLocal(panel->uri())
                          ? Vfs::localPathOf(panel->uri())
                          : QString();
                      if (targetDir.isEmpty()) {
                          QMessageBox::information(this, tr("Save clipboard"),
                                                  tr("This works in local folders."));
                          return;
                      }
                      if (!Vfs::instance().writable(panel->uri())) {
                          QMessageBox::information(this, tr("Read-only"),
                                                  tr("This location is read-only."));
                          return;
                      }

                      const QClipboard *clipboard = QApplication::clipboard();
                      const QMimeData *mime = clipboard->mimeData();
                      QString written;
                      QString error;
                      bool ok = false;
                      if (mime && mime->hasImage()) {
                          ok = ClipboardToFile::saveImage(targetDir, clipboard->image(),
                                                          &written, &error);
                      } else if (mime && mime->hasText()) {
                          ok = ClipboardToFile::saveText(targetDir, mime->text(), &written, &error);
                      } else {
                          QMessageBox::information(this, tr("Save clipboard"),
                                                  tr("The clipboard has no text or image."));
                          return;
                      }
                      if (!ok) {
                          QMessageBox::warning(this, tr("Save clipboard"), error);
                          return;
                      }
                      panel->model()->refresh();
                      panel->selectName(QFileInfo(written).fileName());
                      statusBar()->showMessage(tr("Saved %1").arg(QFileInfo(written).fileName()),
                                               4000);
                  }});

    registry.add({QStringLiteral("file.convertImage"), tr("Convert image…"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      const QStringList uris = panel->selectedUris();
                      if (uris.isEmpty()) {
                          statusBar()->showMessage(tr("Select an image first."), 2500);
                          return;
                      }
                      if (!Vfs::isLocal(panel->uri())) {
                          QMessageBox::information(this, tr("Convert image"),
                                                  tr("This works in local folders."));
                          return;
                      }
                      if (!Vfs::instance().writable(panel->uri())) {
                          QMessageBox::information(this, tr("Read-only"),
                                                  tr("This location is read-only."));
                          return;
                      }

                      bool accepted = false;
                      const QString format = QInputDialog::getItem(
                          this, tr("Convert image"), tr("Format:"),
                          QStringList{QStringLiteral("png"), QStringLiteral("jpg"),
                                      QStringLiteral("webp"), QStringLiteral("bmp"),
                                      QStringLiteral("gif"), QStringLiteral("tiff")},
                          0, false, &accepted);
                      if (!accepted || format.isEmpty())
                          return;

                      const QString dir = Vfs::localPathOf(panel->uri());
                      int converted = 0;
                      for (const QString &uri : uris) {
                          if (!Vfs::isLocal(uri))
                              continue;
                          const QString source = Vfs::localPathOf(uri);
                          if (!ImageDrop::isImageFile(source))
                              continue;
                          const QString baseName = ImageDrop::suggestedName(source, format);
                          const QString target = ClipboardToFile::uniquePath(dir, baseName);
                          QString error;
                          if (!ImageDrop::convertImage(source, target, format, &error)) {
                              QMessageBox::warning(this, tr("Convert image"), error);
                              continue;
                          }
                          ++converted;
                      }
                      panel->model()->refresh();
                      statusBar()->showMessage(
                          tr("Converted %1 image(s).").arg(converted), 3000);
                  }});

    registry.add({QStringLiteral("file.checksums"), tr("Copy checksums…"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      const QStringList uris = panel->selectedUris();
                      if (uris.isEmpty()) {
                          statusBar()->showMessage(tr("Select a file first."), 2500);
                          return;
                      }
                      auto lines = std::make_shared<QStringList>();
                      startTask(tr("Computing checksums"),
                                [uris, lines](FileOps::Progress &progress, QString *) {
                          for (const QString &uri : uris) {
                              if (progress.cancelled && progress.cancelled())
                                  break;
                              if (!Vfs::isLocal(uri)) {
                                  lines->append(tr("%1 (remote, skipped)")
                                                    .arg(QFileInfo(uri).fileName()));
                                  continue;
                              }
                              const QString path = Vfs::localPathOf(uri);
                              if (progress.current)
                                  progress.current(QFileInfo(path).fileName());
                              QString error;
                              const QString sha = FileHash::hexDigest(
                                  path, FileHash::Algorithm::Sha256, &error);
                              if (sha.isEmpty()) {
                                  lines->append(tr("%1: %2")
                                                    .arg(QFileInfo(path).fileName(), error));
                                  continue;
                              }
                              lines->append(tr("%1  %2").arg(sha, QFileInfo(path).fileName()));
                          }
                          return QStringList();
                      }, QString(), [this, lines](const TaskOutcome &outcome) {
                          if (outcome.cancelled || lines->isEmpty())
                              return;
                          const QString text = lines->join(QLatin1Char('\n'));
                          QApplication::clipboard()->setText(text);
                          QMessageBox::information(this, tr("Checksums"), text);
                          statusBar()->showMessage(tr("Checksums copied to clipboard."), 3000);
                      });
                  }});

    registry.add({QStringLiteral("file.secureDelete"), tr("Securely delete"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      const QStringList uris = panel->selectedUris();
                      if (uris.isEmpty()) {
                          statusBar()->showMessage(tr("Select files first."), 2500);
                          return;
                      }
                      const auto answer = QMessageBox::question(
                          this, tr("Securely delete"),
                          tr("Securely delete %1 selected item(s)? This cannot be undone.")
                              .arg(uris.size()),
                          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                      if (answer != QMessageBox::Yes)
                          return;

                      int deleted = 0;
                      QStringList errors;
                      for (const QString &uri : uris) {
                          if (!Vfs::isLocal(uri)) {
                              errors.append(tr("%1: not local").arg(QFileInfo(uri).fileName()));
                              continue;
                          }
                          QString error;
                          const QString path = Vfs::localPathOf(uri);
                          const QFileInfo target(path);
                          // A folder is shredded file by file; a symlink is
                          // removed as a link, never through its target.
                          const bool shredded = target.isDir() && !target.isSymLink()
                              ? FileShred::shredTree(path, 3, &error)
                              : FileShred::shredFile(path, 3, &error);
                          if (shredded)
                              ++deleted;
                          else
                              errors.append(tr("%1: %2").arg(QFileInfo(uri).fileName(), error));
                      }
                      panel->model()->refresh();
                      QString message = tr("Securely deleted %1 item(s).").arg(deleted);
                      if (!errors.isEmpty())
                          message += QLatin1Char('\n') + errors.join(QLatin1Char('\n'));
                      statusBar()->showMessage(message, 5000);
                      if (!errors.isEmpty())
                          QMessageBox::warning(this, tr("Secure delete"), errors.join(QLatin1Char('\n')));
                  }});

    registry.add({QStringLiteral("bookmark.add"), tr("Add bookmark"), tr("Navigation"),
                  [this](CommandContext &) {
                      QSettings settings;
                      QStringList bookmarks = settings.value(QStringLiteral("ui/bookmarks")).toStringList();
                      const QString uri = VfsUri::redactCredentials(activePanel()->uri());
                      if (!bookmarks.contains(uri)) {
                          bookmarks.append(uri);
                          settings.setValue(QStringLiteral("ui/bookmarks"), bookmarks);
                          buildPlaces();
                      }
                  }});

    registry.add({QStringLiteral("bookmark.rename"), tr("Rename bookmark"), tr("Navigation"),
                  [this](CommandContext &) {
                      QSettings settings;
                      QStringList bookmarks = settings.value(QStringLiteral("ui/bookmarks")).toStringList();
                      if (bookmarks.isEmpty())
                          return;
                      bool accepted = false;
                      const QString chosen = QInputDialog::getItem(
                          this, tr("Rename bookmark"), tr("Bookmark:"), bookmarks, 0, false, &accepted);
                      if (!accepted || chosen.isEmpty())
                          return;
                      bool ok = false;
                      const QString newName = QInputDialog::getText(
                          this, tr("Rename bookmark"), tr("New address:"), QLineEdit::Normal, chosen, &ok);
                      if (!ok || newName.trimmed().isEmpty())
                          return;
                      const int idx = bookmarks.indexOf(chosen);
                      if (idx >= 0) {
                          bookmarks[idx] = newName.trimmed();
                          settings.setValue(QStringLiteral("ui/bookmarks"), bookmarks);
                          buildPlaces();
                      }
                  }});

    registry.add({QStringLiteral("bookmark.remove"), tr("Remove bookmark"), tr("Navigation"),
                  [this](CommandContext &) {
                      QSettings settings;
                      QStringList bookmarks = settings.value(QStringLiteral("ui/bookmarks")).toStringList();
                      if (bookmarks.isEmpty())
                          return;
                      bool accepted = false;
                      const QString chosen = QInputDialog::getItem(
                          this, tr("Remove bookmark"), tr("Bookmark:"), bookmarks, 0, false,
                          &accepted);
                      if (!accepted || chosen.isEmpty())
                          return;
                      bookmarks.removeAll(chosen);
                      settings.setValue(QStringLiteral("ui/bookmarks"), bookmarks);
                      buildPlaces();
                  }});

    registry.add({QStringLiteral("tools.find"), tr("Find files…"), tr("Tools"),
                  [this](CommandContext &) { findFiles(); }});

    registry.add({QStringLiteral("tools.connect"), tr("Connect to server…"), tr("Tools"),
                  [this](CommandContext &) {
                      QDialog dialog(this);
                      dialog.setWindowTitle(tr("Connect to server"));
                      auto *layout = new QFormLayout(&dialog);
                      auto *scheme = new QComboBox(&dialog);
                      scheme->addItems({QStringLiteral("ftp"), QStringLiteral("ftpes"),
                                       QStringLiteral("ftps"), QStringLiteral("sftp"),
                                       QStringLiteral("smb")});
                      scheme->setToolTip(tr("ftpes upgrades a plain FTP connection with TLS "
                                            "(explicit FTPS), ftps uses TLS from the first byte "
                                            "(implicit FTPS)."));
                      auto *host = new QLineEdit(&dialog);
                      auto *port = new QLineEdit(&dialog);
                      auto *user = new QLineEdit(&dialog);
                      auto *pass = new QLineEdit(&dialog);
                      pass->setEchoMode(QLineEdit::Password);
                      auto *path = new QLineEdit(QStringLiteral("/"), &dialog);
                      layout->addRow(tr("Protocol"), scheme);
                      layout->addRow(tr("Host"), host);
                      layout->addRow(tr("Port"), port);
                      layout->addRow(tr("User"), user);
                      layout->addRow(tr("Password"), pass);
                      layout->addRow(tr("Path"), path);

                      // SFTP can authenticate with a key instead of (or as well
                      // as) a password. These rows stay out of the way for the
                      // other protocols.
                      auto *sshBox = new QWidget(&dialog);
                      auto *sshForm = new QFormLayout(sshBox);
                      sshForm->setContentsMargins(0, 0, 0, 0);
                      auto *identity = new QLineEdit(sshBox);
                      identity->setPlaceholderText(SshAuth::defaultIdentityFile());
                      auto *identityBrowse = new QPushButton(tr("Browse…"), sshBox);
                      auto *identityRow = new QWidget(sshBox);
                      auto *identityLayout = new QHBoxLayout(identityRow);
                      identityLayout->setContentsMargins(0, 0, 0, 0);
                      identityLayout->addWidget(identity, 1);
                      identityLayout->addWidget(identityBrowse);
                      auto *knownHosts = new QLineEdit(sshBox);
                      knownHosts->setPlaceholderText(
                          QDir::homePath() + QStringLiteral("/.ssh/known_hosts"));
                      auto *knownHostsBrowse = new QPushButton(tr("Browse…"), sshBox);
                      auto *knownHostsRow = new QWidget(sshBox);
                      auto *knownHostsLayout = new QHBoxLayout(knownHostsRow);
                      knownHostsLayout->setContentsMargins(0, 0, 0, 0);
                      knownHostsLayout->addWidget(knownHosts, 1);
                      knownHostsLayout->addWidget(knownHostsBrowse);
                      auto *passphrase = new QLineEdit(sshBox);
                      passphrase->setEchoMode(QLineEdit::Password);
                      sshForm->addRow(tr("Private key"), identityRow);
                      sshForm->addRow(tr("Known hosts"), knownHostsRow);
                      sshForm->addRow(tr("Key passphrase"), passphrase);
                      layout->addRow(sshBox);

                      const QSettings saved;
                      identity->setText(saved.value(QStringLiteral("remote/sshIdentityFile")).toString());
                      knownHosts->setText(saved.value(QStringLiteral("remote/sshKnownHosts")).toString());

                      const auto updateSshRows = [sshBox, scheme]() {
                          sshBox->setVisible(scheme->currentText() == QLatin1String("sftp"));
                      };
                      connect(scheme, &QComboBox::currentTextChanged, sshBox,
                              [updateSshRows](const QString &) { updateSshRows(); });
                      connect(identityBrowse, &QPushButton::clicked, &dialog, [&]() {
                          const QString file = QFileDialog::getOpenFileName(
                              &dialog, tr("SSH private key"),
                              QDir::homePath() + QStringLiteral("/.ssh"));
                          if (!file.isEmpty())
                              identity->setText(file);
                      });
                      connect(knownHostsBrowse, &QPushButton::clicked, &dialog, [&]() {
                          const QString file = QFileDialog::getOpenFileName(
                              &dialog, tr("Known hosts file"),
                              QDir::homePath() + QStringLiteral("/.ssh"));
                          if (!file.isEmpty())
                              knownHosts->setText(file);
                      });
                      updateSshRows();

                      auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                      layout->addRow(buttons);
                      connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                      connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                      if (dialog.exec() != QDialog::Accepted)
                          return;

                      if (scheme->currentText() == QLatin1String("sftp")) {
                          SshAuth::Options options;
                          options.identityFile = identity->text().trimmed();
                          options.knownHostsFile = knownHosts->text().trimmed();
                          options.publicKeyFile = SshAuth::publicKeyFor(options.identityFile);
                          options.passphrase = passphrase->text();
                          QString sshError;
                          if (!SshAuth::validate(options, &sshError)) {
                              QMessageBox::warning(this, tr("SSH settings"), sshError);
                              return;
                          }
                          QSettings connectionSettings;
                          connectionSettings.setValue(QStringLiteral("remote/sshIdentityFile"),
                                                      options.identityFile);
                          connectionSettings.setValue(QStringLiteral("remote/sshKnownHosts"),
                                                      options.knownHostsFile);
                          setSshOptions(options);
                      }

                      QString error;
                      const QString uri = RemoteUri::build(scheme->currentText(), host->text(),
                                                           port->text(), user->text(),
                                                           pass->text(), path->text(), &error);
                      if (uri.isEmpty()) {
                          QMessageBox::warning(this, tr("Connect to server"), error);
                          return;
                      }
                      activePanel()->setUri(uri);
                      activePanel()->focusLister();
                  }});

    registry.add({QStringLiteral("tools.compare"), tr("Compare folders"), tr("Tools"),
                  [this](CommandContext &) { compareFolders(); }});

    registry.add({QStringLiteral("tools.findDuplicates"), tr("Find duplicate files…"), tr("Tools"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      if (!Vfs::isLocal(panel->uri())) {
                          QMessageBox::information(this, tr("Find duplicates"),
                                                  tr("This works in local folders."));
                          return;
                      }
                      const QString root = Vfs::localPathOf(panel->uri());
                      auto groups = std::make_shared<QList<DuplicateFinder::Group>>();
                      startTask(tr("Finding duplicate files"),
                                [root, groups](FileOps::Progress &progress, QString *) {
                          *groups = DuplicateFinder::findDuplicates(
                              root, 1, [&progress]() {
                                  return progress.cancelled && progress.cancelled();
                              });
                          return QStringList();
                      }, QString(), [this, root, groups](const TaskOutcome &outcome) {
                          if (outcome.cancelled)
                              return;
                          if (groups->isEmpty()) {
                              QMessageBox::information(this, tr("Find duplicates"),
                                                      tr("No duplicate files found."));
                              return;
                          }
                          QDialog dialog(this);
                          dialog.setWindowTitle(tr("Duplicate files"));
                          dialog.resize(720, 420);
                          auto *layout = new QVBoxLayout(&dialog);
                          auto *list = new QListWidget(&dialog);
                          for (const auto &group : *groups) {
                              auto *header = new QListWidgetItem(
                                  tr("%1 bytes — %2 files — %3")
                                      .arg(group.size)
                                      .arg(group.paths.size())
                                      .arg(group.digest.left(12)),
                                  list);
                              QFont bold = header->font();
                              bold.setBold(true);
                              header->setFont(bold);
                              for (const QString &path : group.paths) {
                                  auto *item = new QListWidgetItem(
                                      QStringLiteral("    ") + QDir(root).relativeFilePath(path),
                                      list);
                                  item->setData(Qt::UserRole, path);
                              }
                          }
                          layout->addWidget(list, 1);
                          auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
                          layout->addWidget(buttons);
                          connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                          connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                          dialog.exec();
                      });
                  }});

    registry.add({QStringLiteral("tools.compareFiles"), tr("Compare two files…"), tr("Tools"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      const QStringList uris = panel->selectedUris();
                      if (uris.size() != 2) {
                          QMessageBox::information(this, tr("Compare files"),
                                                  tr("Select exactly two files."));
                          return;
                      }
                      if (!Vfs::isLocal(uris.at(0)) || !Vfs::isLocal(uris.at(1))) {
                          QMessageBox::information(this, tr("Compare files"),
                                                  tr("This works in local folders."));
                          return;
                      }
                      const QString path1 = Vfs::localPathOf(uris.at(0));
                      const QString path2 = Vfs::localPathOf(uris.at(1));
                      // Refuse before reading: the size check used to run after
                      // both files had already been read and decoded.
                      constexpr qint64 kMaxCompareBytes = 4 * 1024 * 1024;
                      if (QFileInfo(path1).size() > kMaxCompareBytes
                          || QFileInfo(path2).size() > kMaxCompareBytes) {
                          QMessageBox::information(this, tr("Compare files"),
                                                  tr("Files are too large to compare."));
                          return;
                      }
                      QFile f1(path1);
                      QFile f2(path2);
                      if (!f1.open(QIODevice::ReadOnly) || !f2.open(QIODevice::ReadOnly)) {
                          QMessageBox::warning(this, tr("Compare files"), tr("Could not read the files."));
                          return;
                      }
                      const QByteArray b1 = f1.readAll();
                      const QByteArray b2 = f2.readAll();
                      f1.close();
                      f2.close();

                      // Invalid UTF-8 used to become U+FFFD on both sides, so
                      // two different Latin-1 letters compared equal. Detect
                      // it and offer an explicit byte-preserving decoding.
                      bool latin1 = false;
                      if (!TextDiff::isValidUtf8(b1) || !TextDiff::isValidUtf8(b2)) {
                          const auto answer = QMessageBox::question(
                              this, tr("Compare files"),
                              tr("These files are not valid UTF-8. Compare them as Latin-1 "
                                 "instead? Lines that differ in other encodings cannot be told "
                                 "apart byte for byte."),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                          if (answer != QMessageBox::Yes)
                              return;
                          latin1 = true;
                      }
                      const QString t1 = TextDiff::decodeText(b1, latin1);
                      const QString t2 = TextDiff::decodeText(b2, latin1);
                      const TextDiff::Diff diff = TextDiff::compare(t1, t2);
                      if (!diff.ok) {
                          QMessageBox::warning(this, tr("Compare files"), diff.error);
                          return;
                      }
                      QDialog dialog(this);
                      dialog.setWindowTitle(tr("File comparison"));
                      dialog.resize(760, 520);
                      auto *layout = new QVBoxLayout(&dialog);
                      auto *edit = new QPlainTextEdit(&dialog);
                      edit->setReadOnly(true);
                      edit->setPlainText(diff.text);
                      layout->addWidget(edit, 1);
                      auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
                      layout->addWidget(buttons);
                      connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                      connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                      dialog.exec();
                  }});

    registry.add({QStringLiteral("tools.editExternal"), tr("Edit external tools…"), tr("Tools"),
                  [this](CommandContext &) {
                      QSettings settings;
                      QStringList entries = settings.value(QStringLiteral("tools/external")).toStringList();
                      QDialog dialog(this);
                      dialog.setWindowTitle(tr("External tools"));
                      dialog.resize(520, 360);
                      auto *layout = new QVBoxLayout(&dialog);
                      auto *list = new QListWidget(&dialog);
                      for (const QString &entry : entries)
                          list->addItem(entry);
                      layout->addWidget(list, 1);

                      auto *row = new QHBoxLayout();
                      auto *addButton = new QPushButton(tr("Add"), &dialog);
                      auto *editButton = new QPushButton(tr("Edit"), &dialog);
                      auto *removeButton = new QPushButton(tr("Remove"), &dialog);
                      row->addWidget(addButton);
                      row->addWidget(editButton);
                      row->addWidget(removeButton);
                      row->addStretch();
                      layout->addLayout(row);

                      auto refreshEntries = [&]() {
                          entries.clear();
                          for (int i = 0; i < list->count(); ++i)
                              entries.append(list->item(i)->text());
                      };
                      connect(addButton, &QPushButton::clicked, &dialog, [&]() {
                          bool ok = false;
                          const QString text = QInputDialog::getText(
                              &dialog, tr("Add tool"),
                              tr("name|program|args"), QLineEdit::Normal,
                              QStringLiteral("My tool|/usr/bin/xdg-open|%d"), &ok);
                          if (ok && !text.trimmed().isEmpty())
                              list->addItem(text.trimmed());
                      });
                      connect(editButton, &QPushButton::clicked, &dialog, [&]() {
                          const int row = list->currentRow();
                          if (row < 0)
                              return;
                          bool ok = false;
                          const QString text = QInputDialog::getText(
                              &dialog, tr("Edit tool"), tr("name|program|args"),
                              QLineEdit::Normal, list->item(row)->text(), &ok);
                          if (ok && !text.trimmed().isEmpty())
                              list->item(row)->setText(text.trimmed());
                      });
                      connect(removeButton, &QPushButton::clicked, &dialog, [&]() {
                          const int row = list->currentRow();
                          if (row >= 0)
                              delete list->takeItem(row);
                      });

                      auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                      layout->addWidget(buttons);
                      connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                      connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

                      if (dialog.exec() != QDialog::Accepted)
                          return;

                      refreshEntries();
                      settings.setValue(QStringLiteral("tools/external"), entries);
                      registerExternalTools();
                      buildMenus();
                      statusBar()->showMessage(tr("External tools updated."), 3000);
                  }});

    registry.add({QStringLiteral("tools.terminal"), tr("Open terminal here"), tr("Tools"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      QString dir = Vfs::isLocal(panel->uri()) ? Vfs::localPathOf(panel->uri())
                                                              : QDir::homePath();
                      QString error;
                      if (!TerminalLauncher::launch(dir, &error))
                          QMessageBox::warning(this, tr("Open terminal here"), error);
                  }});


    registry.add({QStringLiteral("file.properties"), tr("Properties"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      const QStringList uris = panel->selectedUris();
                      if (uris.isEmpty())
                          return;

                      // Only local files can have their permissions edited; for
                      // remote/archive entries we keep the read-only summary.
                      bool anyLocal = false;
                      for (const QString &uri : uris)
                          if (Vfs::isLocal(uri)) anyLocal = true;

                      if (!anyLocal) {
                          for (const QString &uri : uris) {
                              QString local = Vfs::isLocal(uri) ? Vfs::localPathOf(uri) : QString();
                              if (local.isEmpty()) {
                                  QString error;
                                  local = TemporaryCopy::ofUri(uri, &error);
                                  if (local.isEmpty()) {
                                      QMessageBox::warning(this, tr("Properties"), error);
                                      continue;
                                  }
                              }
                              const QFileInfo info(local);
                              QMessageBox::information(
                                  this, tr("Properties — %1").arg(info.fileName()),
                                  tr("Location: %1\nSize: %2\nModified: %3\nPermissions: %4")
                                      .arg(Vfs::isLocal(uri) ? info.absolutePath() : uri,
                                           humanSize(info.size()),
                                           info.lastModified().toString(),
                                           QString::number(static_cast<int>(info.permissions()), 16)));
                          }
                          return;
                      }

                      QStringList paths;
                      paths.reserve(uris.size());
                      for (const QString &uri : uris)
                          paths.append(Vfs::localPathOf(uri));

                      const PropertiesBatch::Summary summary = PropertiesBatch::summarize(paths);
                      const QFileInfo firstInfo(paths.first());

                      QDialog dialog(this);
                      dialog.setWindowTitle(
                          paths.size() == 1
                              ? tr("Properties — %1").arg(firstInfo.fileName())
                              : tr("Properties — %1 items").arg(paths.size()));
                      auto *layout = new QVBoxLayout(&dialog);
                      layout->addWidget(new QLabel(
                          tr("Location: %1")
                              .arg(summary.location.isEmpty() ? tr("Multiple folders")
                                                              : summary.location),
                          &dialog));
                      if (paths.size() == 1) {
                          layout->addWidget(
                              new QLabel(tr("Size: %1").arg(humanSize(firstInfo.size())), &dialog));
                          layout->addWidget(new QLabel(
                              tr("Modified: %1").arg(firstInfo.lastModified().toString()), &dialog));
                      } else {
                          layout->addWidget(new QLabel(
                              tr("%1 in %2 files and %3 folders")
                                  .arg(humanSize(summary.totalSize))
                                  .arg(summary.files)
                                  .arg(summary.folders),
                              &dialog));
                      }

                      auto *group = new QGroupBox(tr("Permissions"), &dialog);
                      auto *grid = new QGridLayout(group);
                      const QStringList columns = {tr("Read"), tr("Write"), tr("Execute")};
                      const QStringList rows = {tr("Owner"), tr("Group"), tr("Other")};
                      const QFile::Permissions table[3][3] = {
                          {QFile::ReadOwner, QFile::WriteOwner, QFile::ExeOwner},
                          {QFile::ReadGroup, QFile::WriteGroup, QFile::ExeGroup},
                          {QFile::ReadOther, QFile::WriteOther, QFile::ExeOther},
                      };
                      for (int c = 0; c < 3; ++c)
                          grid->addWidget(new QLabel(columns[c], group), 0, c + 1);
                      QCheckBox *boxes[3][3] = {};
                      QVector<PropertiesBatch::BoxState> initialStates;
                      QVector<QFile::Permissions> permissionBits;
                      for (int r = 0; r < 3; ++r) {
                          grid->addWidget(new QLabel(rows[r], group), r + 1, 0);
                          for (int c = 0; c < 3; ++c) {
                              // Mixed selections show a partially checked box; a
                              // partial box leaves that bit unchanged on accept.
                              bool anySet = false;
                              bool anyClear = false;
                              for (const QString &path : paths) {
                                  if (QFileInfo(path).permissions() & table[r][c])
                                      anySet = true;
                                  else
                                      anyClear = true;
                              }
                              auto *box = new QCheckBox(group);
                              box->setTristate(true);
                              const Qt::CheckState state = anySet && anyClear ? Qt::PartiallyChecked
                                                          : anySet              ? Qt::Checked
                                                                                : Qt::Unchecked;
                              box->setCheckState(state);
                              boxes[r][c] = box;
                              initialStates.append(state == Qt::Checked
                                                       ? PropertiesBatch::BoxState::On
                                                   : state == Qt::Unchecked
                                                       ? PropertiesBatch::BoxState::Off
                                                       : PropertiesBatch::BoxState::Mixed);
                              permissionBits.append(table[r][c]);
                              grid->addWidget(box, r + 1, c + 1);
                          }
                      }
                      layout->addWidget(group);

                      auto *recursive =
                          new QCheckBox(tr("Apply to folder contents (recursive)"), &dialog);
                      recursive->setEnabled(summary.folders > 0);
                      layout->addWidget(recursive);

                      auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                      layout->addWidget(buttons);
                      connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                      connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

                      if (dialog.exec() != QDialog::Accepted)
                          return;

                      QVector<PropertiesBatch::BoxState> finalStates;
                      for (int r = 0; r < 3; ++r) {
                          for (int c = 0; c < 3; ++c) {
                              const Qt::CheckState state = boxes[r][c]->checkState();
                              finalStates.append(state == Qt::Checked
                                                     ? PropertiesBatch::BoxState::On
                                                 : state == Qt::Unchecked
                                                     ? PropertiesBatch::BoxState::Off
                                                     : PropertiesBatch::BoxState::Mixed);
                          }
                      }
                      // Only boxes the user changed become explicit set/clear
                      // operations; untouched boxes keep each child's own bit.
                      const PropertiesBatch::Change change = PropertiesBatch::changesFromStates(
                          initialStates, finalStates, permissionBits);
                      const QStringList failures =
                          PropertiesBatch::apply(paths, change, recursive->isChecked());
                      panel->model()->refresh();
                      if (!failures.isEmpty()) {
                          QMessageBox::warning(
                              this, tr("Properties"),
                              tr("%1 permission changes failed:\n%2")
                                  .arg(failures.size())
                                  .arg(failures.mid(0, 8).join(QLatin1Char('\n'))));
                      }
                  }});

    registry.add({QStringLiteral("file.restore"), tr("Restore from trash"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      const QStringList uris = panel->selectedUris();
                      if (uris.isEmpty())
                          return;

                      // Restore is tied to the actual trash entry. In an
                      // ordinary folder the recorded original path decides
                      // which item is meant, not a matching file name.
                      const QString current = Vfs::isLocal(panel->uri())
                          ? Vfs::localPathOf(panel->uri())
                          : QString();
                      const bool inTrash
                          = TransferPlan::isInside(current, FileOps::trashFolderPath());

                      QString error;
                      bool restored = false;
                      if (inTrash) {
                          QStringList names;
                          for (const QString &uri : uris)
                              names.append(vfsLeafName(uri));
                          restored = FileOps::restoreFromTrash(names, &error);
                      } else {
                          QStringList paths;
                          for (const QString &uri : uris) {
                              if (!Vfs::isLocal(uri)) {
                                  error = tr("Only local items can be restored from the trash.");
                                  break;
                              }
                              paths.append(Vfs::localPathOf(uri));
                          }
                          if (error.isEmpty())
                              restored = FileOps::restoreLocalPaths(paths, nullptr, &error);
                      }

                      if (!restored) {
                          QMessageBox::warning(this, tr("Restore"),
                                              error.isEmpty() ? tr("Could not restore.") : error);
                          return;
                      }
                      panel->model()->refresh();
                      statusBar()->showMessage(tr("Restored."), 4000);
                  }});

    registry.add({QStringLiteral("file.emptyTrash"), tr("Empty trash"), tr("Files"),
                  [this](CommandContext &) {
                      const auto answer = QMessageBox::question(
                          this, tr("Empty trash"), tr("Permanently delete everything in the trash?"),
                          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                      if (answer != QMessageBox::Yes)
                          return;
                      QString error;
                      FileOps::emptyTrash(&error);
                      activePanel()->model()->refresh();
                      if (!error.isEmpty())
                          QMessageBox::warning(this, tr("Empty trash"), error);
                      else
                          statusBar()->showMessage(tr("Trash emptied."), 4000);
                  }});

    registry.add({QStringLiteral("file.duplicate"), tr("Duplicate"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      if (!Vfs::isLocal(panel->uri())) {
                          QMessageBox::information(this, tr("Duplicate"),
                                                  tr("Duplicate works in local folders."));
                          return;
                      }
                      const QStringList uris = panel->selectedUris();
                      QStringList failures;
                      for (const QString &uri : uris) {
                          const QString src = Vfs::localPathOf(uri);
                          const QFileInfo info(src);
                          const QString dir = info.absolutePath();
                          QString base = info.completeBaseName().isEmpty() ? info.fileName()
                                                                          : info.completeBaseName();
                          const QString suffix = info.suffix().isEmpty()
                              ? QString() : QStringLiteral(".") + info.suffix();
                          QString dest = QDir(dir).filePath(base + QStringLiteral(" copy") + suffix);
                          int n = 2;
                          while (QFileInfo::exists(dest)) {
                              dest = QDir(dir).filePath(base + QStringLiteral(" copy ") + QString::number(n++) + suffix);
                          }

                          // The same staged, verified copy as the panel
                          // commands; a failed duplicate is reported instead of
                          // being dropped on the floor.
                          QString copyError;
                          if (FileOps::copyRecursively(src, dest, &copyError, FileOps::Progress())
                              != FileOps::CopyOk) {
                              failures.append(copyError.isEmpty()
                                                  ? tr("%1 could not be duplicated").arg(info.fileName())
                                                  : copyError);
                          }
                      }
                      panel->model()->refresh();
                      if (!failures.isEmpty())
                          QMessageBox::warning(this, tr("Duplicate"),
                                              failures.join(QLatin1Char('\n')));
                  }});

    registry.add({QStringLiteral("file.symlink"), tr("Create link"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      if (!Vfs::isLocal(panel->uri())) {
                          QMessageBox::information(this, tr("Link"), tr("Links are local only."));
                          return;
                      }
                      const QStringList uris = panel->selectedUris();
                      if (uris.isEmpty())
                          return;
                      bool accepted = false;
                      const QString name = QInputDialog::getText(
                          this, tr("Create link"), tr("Link name:"), QLineEdit::Normal,
                          vfsLeafName(uris.first()) + QStringLiteral(".lnk"), &accepted);
                      if (!accepted || name.trimmed().isEmpty())
                          return;
                      QFile::link(Vfs::localPathOf(uris.first()),
                                  QDir(Vfs::localPathOf(panel->uri())).filePath(name.trimmed()));
                      panel->model()->refresh();
                  }});

    registry.add({QStringLiteral("edit.undo"), tr("Undo"), tr("Edit"),
                  [this](CommandContext &) {
                      // Undoing an upload reads the copies back from an archive
                      // or server, so it runs on the same worker as a transfer.
                      UndoStack::Entry top;
                      if (m_undoStack.peek(&top)
                          && (top.type == UndoStack::OpType::Upload
                              || top.type == UndoStack::OpType::MoveOut
                              || top.type == UndoStack::OpType::RemoteMove)) {
                          const UndoStack::Entry entry = top;
                          startTask(tr("Undoing the transfer"),
                                    [entry](FileOps::Progress &progress, QString *error) {
                              FileOps::Result result;
                              switch (entry.type) {
                              case UndoStack::OpType::Upload:
                                  result = FileOps::undoUploadsResult(entry.uploads, error, progress);
                                  break;
                              case UndoStack::OpType::MoveOut:
                                  result = FileOps::undoMovesOutResult(entry.movesOut, error, progress);
                                  break;
                              default:
                                  result = FileOps::undoRemoteMovesResult(entry.movesRemote, error,
                                                                         progress);
                                  break;
                              }
                              return result.failed;
                          }, QString(), [this](const TaskOutcome &outcome) {
                              if (outcome.cancelled || !outcome.failed.isEmpty()
                                  || !outcome.error.isEmpty()) {
                                  return; // the entry stays so it can be retried
                              }
                              m_undoStack.pop();
                              m_source->model()->refresh();
                              m_dest->model()->refresh();
                              statusBar()->showMessage(tr("Undone."), 3000);
                          });
                          return;
                      }
                      QString error;
                      if (!undoLast(&error)) {
                          statusBar()->showMessage(error, 4000);
                          return;
                      }
                      m_source->model()->refresh();
                      m_dest->model()->refresh();
                      statusBar()->showMessage(tr("Undone."), 3000);
                  }});

    registry.add({QStringLiteral("view.singlePanel"), tr("Single panel"), tr("View"),
                  [this](CommandContext &) {
                      applyWorkspace(m_folderTreeDock && m_folderTreeDock->isVisible()
                                         ? Workspace::Askr
                                         : Workspace::Einn);
                  }});

    registry.add({QStringLiteral("view.twoPanels"), tr("Two panels"), tr("View"),
                  [this](CommandContext &) {
                      applyWorkspace(m_folderTreeDock && m_folderTreeDock->isVisible()
                                         ? Workspace::Smidja
                                         : Workspace::Tviskipt);
                  }});

    registry.add({QStringLiteral("app.palette"), tr("Command Palette"), tr("Application"),
                  [this](CommandContext &) {
                      const QString id = CommandPalette::pick(this);
                      if (id.isEmpty())
                          return;
                      CommandContext context;
                      context.sourcePath = m_source->uri();
                      context.destPath = m_dest->uri();
                      context.selection = activePanel()->selection();
                      CommandRegistry::instance().run(id, context);
                  }});

    registry.add({QStringLiteral("app.preferences"), tr("Preferences…"), tr("Application"),
                  [this](CommandContext &) { openPreferences(); }});

    registry.add({QStringLiteral("help.shortcuts"), tr("Keyboard Shortcuts"), tr("Help"),
                  [this](CommandContext &) { ShortcutSheet::showSheet(this); }});

    registry.add({QStringLiteral("help.welcome"), tr("Show the introduction again"), tr("Help"),
                  [this](CommandContext &) { showWelcome(); }});

    registry.add({QStringLiteral("help.about"), tr("About Listista"), tr("Help"),
                  [this](CommandContext &) { showAbout(); }});

    registry.add({QStringLiteral("view.toggleFolderTree"), tr("Show or hide folder tree"), tr("View"),
                  [this](CommandContext &) {
                      if (m_folderTreeDock)
                          m_folderTreeDock->setVisible(!m_folderTreeDock->isVisible());
                  }});

    registry.add({QStringLiteral("view.togglePlaces"), tr("Show or hide places"), tr("View"),
                  [this](CommandContext &) { togglePlaces(); }});

    registry.add({QStringLiteral("file.collectToShelf"), tr("Send to Shelf"), tr("Files"),
                  [this](CommandContext &context) {
                      Panel *panel = activePanel();
                      const QModelIndexList rows
                          = panel->view()->selectionModel()->selectedIndexes();
                      Q_UNUSED(context)
                      for (const QModelIndex &index : rows) {
                          Shelf::Entry entry;
                          entry.uri = index.data(DirModel::UriRole).toString();
                          entry.name = index.data(Qt::DisplayRole).toString();
                          entry.isDir = index.data(DirModel::IsDirRole).toBool();
                          m_shelf.addUnique(entry);
                      }
                      refreshShelfView();
                      if (m_shelfDock && !m_shelfDock->isVisible())
                          m_shelfDock->show();
                      statusBar()->showMessage(
                          tr("Shelf holds %1 entries.").arg(m_shelf.count()), 3000);
                  }});

    registry.add({QStringLiteral("view.showShelf"), tr("Show or hide Shelf"), tr("View"),
                  [this](CommandContext &) {
                      if (!m_shelfDock)
                          return;
                      m_shelfDock->setVisible(!m_shelfDock->isVisible());
                      if (m_shelfDock->isVisible())
                          refreshShelfView();
                  }});

    registry.add({QStringLiteral("view.clearShelf"), tr("Clear Shelf"), tr("View"),
                  [this](CommandContext &) {
                      m_shelf.clear();
                      refreshShelfView();
                      statusBar()->showMessage(tr("Shelf cleared."), 2500);
                  }});

    registry.add({QStringLiteral("shelf.saveCollection"), tr("Save Shelf as collection…"), tr("Shelf"),
                  [this](CommandContext &) {
                      if (m_shelf.count() == 0) {
                          statusBar()->showMessage(tr("Shelf is empty."), 2500);
                          return;
                      }
                      bool ok = false;
                      const QString name = QInputDialog::getText(
                          this, tr("Save Shelf collection"), tr("Collection name:"),
                          QLineEdit::Normal, QString(), &ok);
                      if (!ok || name.trimmed().isEmpty())
                          return;
                      QSettings settings;
                      settings.beginGroup(QStringLiteral("shelf/collections"));
                      settings.setValue(name.trimmed(),
                                        Shelf::encodeEntries(m_shelf.entries()));
                      settings.endGroup();
                      statusBar()->showMessage(
                          tr("Saved Shelf collection “%1”." ).arg(name.trimmed()), 3000);
                  }});

    registry.add({QStringLiteral("shelf.loadCollection"), tr("Load Shelf collection…"), tr("Shelf"),
                  [this](CommandContext &) {
                      QSettings settings;
                      settings.beginGroup(QStringLiteral("shelf/collections"));
                      const QStringList names = settings.childKeys();
                      settings.endGroup();
                      if (names.isEmpty()) {
                          QMessageBox::information(this, tr("Load Shelf collection"),
                                                  tr("No saved Shelf collections."));
                          return;
                      }
                      bool ok = false;
                      const QString name = QInputDialog::getItem(
                          this, tr("Load Shelf collection"), tr("Collection:"), names, 0, false, &ok);
                      if (!ok || name.isEmpty())
                          return;
                      settings.beginGroup(QStringLiteral("shelf/collections"));
                      const QStringList encoded = settings.value(name).toStringList();
                      settings.endGroup();
                      m_shelf.clear();
                      for (const Shelf::Entry &entry : Shelf::decodeEntries(encoded))
                          m_shelf.add(entry);
                      refreshShelfView();
                      statusBar()->showMessage(
                          tr("Loaded %1 entries from “%2”." ).arg(m_shelf.count()).arg(name), 3000);
                  }});

    registry.add({QStringLiteral("view.toggleOperations"), tr("Show or hide Operations"), tr("View"),
                  [this](CommandContext &) {
                      if (!m_operationsDock)
                          return;
                      m_operationsDock->setVisible(!m_operationsDock->isVisible());
                  }});

    registry.add({QStringLiteral("view.toggleShell"), tr("Show or hide shell"), tr("View"),
                  [this](CommandContext &) {
                      if (!m_shellDock)
                          return;
                      const bool show = !m_shellDock->isVisible();
                      m_shellDock->setVisible(show);
                      if (!show)
                          return;
                      syncShellToActivePanel();
                      auto *page = qobject_cast<ShellWidget *>(m_shellStack->currentWidget());
                      if (page)
                          page->focusInput();
                      statusBar()->showMessage(
                          tr("Shell runs in the active panel's folder; drag the dock out to detach it."),
                          4000);
                  }});

    registry.add({QStringLiteral("view.flat"), tr("Flat View"), tr("View"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      if (!panel)
                          return;
                      const bool flat = !panel->flatView();
                      panel->setFlatView(flat);
                      statusBar()->showMessage(flat ? tr("Flat View: showing the whole local tree.")
                                                    : tr("Flat View off."), 3000);
                  }});

    registry.add({QStringLiteral("view.millerColumns"), tr("Miller columns view"), tr("View"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      if (!panel)
                          return;
                      const bool miller = !panel->millerView();
                      panel->setMillerView(miller);
                      statusBar()->showMessage(miller ? tr("Miller columns view.")
                                                      : tr("Details view."), 2500);
                  }});

    registry.add({QStringLiteral("shelf.copyToFolder"), tr("Copy Shelf to this folder"), tr("View"),
                  [this](CommandContext &) {
                      if (m_shelf.count() == 0) {
                          statusBar()->showMessage(tr("Shelf is empty."), 2500);
                          return;
                      }
                      QVector<FileOps::CopyItem> items;
                      items.reserve(m_shelf.count());
                      for (const Shelf::Entry &entry : m_shelf.entries()) {
                          FileOps::CopyItem item;
                          item.uri = entry.uri;
                          item.isDir = entry.isDir;
                          items.append(item);
                      }
                      runTransfer(items, activePanel()->uri(), false);
                  }});

    registry.add({QStringLiteral("shelf.moveToFolder"), tr("Move Shelf to this folder"), tr("View"),
                  [this](CommandContext &) {
                      if (m_shelf.count() == 0) {
                          statusBar()->showMessage(tr("Shelf is empty."), 2500);
                          return;
                      }
                      QVector<FileOps::CopyItem> items;
                      items.reserve(m_shelf.count());
                      for (const Shelf::Entry &entry : m_shelf.entries()) {
                          FileOps::CopyItem item;
                          item.uri = entry.uri;
                          item.isDir = entry.isDir;
                          items.append(item);
                      }
                      runTransfer(items, activePanel()->uri(), true);
                  }});

    registry.add({QStringLiteral("view.jobReports"), tr("Job reports…"), tr("View"),
                  [this](CommandContext &) {
                      if (m_jobReports.count() == 0) {
                          QMessageBox::information(this, tr("Job reports"),
                                                   tr("No jobs have run yet."));
                          return;
                      }
                      QStringList lines;
                      for (const JobReport::Report &report : m_jobReports.all()) {
                          QString state;
                          switch (report.state) {
                          case JobReport::State::Waiting:
                              state = tr("waiting");
                              break;
                          case JobReport::State::Running:
                              state = tr("running");
                              break;
                          case JobReport::State::Completed:
                              state = tr("completed");
                              break;
                          case JobReport::State::Failed:
                              state = tr("failed");
                              break;
                          case JobReport::State::Cancelled:
                              state = tr("cancelled");
                              break;
                          }
                          if (report.itemsTotal > 0)
                              state += tr(" — %1/%2 items")
                                           .arg(report.itemsDone)
                                           .arg(report.itemsTotal);
                          if (!report.error.isEmpty())
                              state += QStringLiteral(" — ") + report.error;
                          lines << tr("%1: %2 (%3)").arg(report.title, state, report.id);
                      }
                      QMessageBox::information(this, tr("Job reports"),
                                               lines.join(QLatin1Char('\n')));
                  }});

    registry.add({QStringLiteral("view.clearJobReports"), tr("Clear finished job reports"),
                  tr("View"), [this](CommandContext &) {
                      m_jobReports.clearCompleted();
                      refreshOperationsView();
                      statusBar()->showMessage(tr("Finished job reports cleared."), 2500);
                  }});

    registry.add({QStringLiteral("view.saveLayout"), tr("Save workspace layout…"), tr("View"),
                  [this](CommandContext &) {
                      bool accepted = false;
                      const QString name = QInputDialog::getText(
                          this, tr("Save layout"), tr("Layout name:"), QLineEdit::Normal,
                          tr("My layout"), &accepted);
                      if (!accepted || !LayoutStore::validName(name))
                          return;
                      LayoutStore::Layout layout;
                      layout.name = name.trimmed();
                      layout.panelCount = singlePaneWorkspace() ? 1 : 2;
                      layout.folderTree = m_folderTreeDock && m_folderTreeDock->isVisible();
                      layout.places = m_placesBox && m_placesBox->isVisible();
                      layout.sourceUri = m_source->uri();
                      layout.destUri = m_dest->uri();
                      QSettings settings;
                      const QString key = LayoutStore::keyFor(layout.name);
                      // Re-saving a name migrates an older space->underscore
                      // entry instead of leaving a second, stale copy behind.
                      settings.beginGroup(QStringLiteral("layouts"));
                      const QStringList existing = settings.childKeys();
                      for (const QString &oldKey : existing) {
                          if (oldKey == key)
                              continue;
                          LayoutStore::Layout old;
                          if (LayoutStore::deserialize(settings.value(oldKey).toString(), &old)
                              && old.name == layout.name) {
                              settings.remove(oldKey);
                          }
                      }
                      settings.endGroup();
                      settings.setValue(QStringLiteral("layouts/") + key,
                                        LayoutStore::serialize(layout));
                      statusBar()->showMessage(tr("Layout “%1” saved.").arg(layout.name), 3000);
                  }});

    registry.add({QStringLiteral("view.applyLayout"), tr("Apply workspace layout…"), tr("View"),
                  [this](CommandContext &) {
                      QSettings settings;
                      settings.beginGroup(QStringLiteral("layouts"));
                      const QStringList keys = settings.childKeys();
                      settings.endGroup();
                      if (keys.isEmpty()) {
                          QMessageBox::information(this, tr("Apply layout"),
                                                   tr("No saved layouts yet."));
                          return;
                      }
                      // Show the stored display name, not the settings key, so
                      // "Min layout" stays readable and both similar names can
                      // coexist.
                      QMap<QString, QString> keyByName;
                      for (const QString &storedKey : keys) {
                          LayoutStore::Layout known;
                          if (!LayoutStore::deserialize(
                                  settings.value(QStringLiteral("layouts/") + storedKey).toString(),
                                  &known)) {
                              continue;
                          }
                          const auto found = keyByName.constFind(known.name);
                          if (found == keyByName.constEnd() || storedKey.startsWith(QStringLiteral("id-")))
                              keyByName.insert(known.name, storedKey);
                      }
                      if (keyByName.isEmpty()) {
                          QMessageBox::information(this, tr("Apply layout"),
                                                   tr("No saved layouts yet."));
                          return;
                      }
                      const QStringList names = keyByName.keys();
                      bool accepted = false;
                      const QString chosen = QInputDialog::getItem(
                          this, tr("Apply layout"), tr("Layout:"), names, 0, false, &accepted);
                      if (!accepted || chosen.isEmpty())
                          return;
                      const QString key = keyByName.value(chosen);
                      LayoutStore::Layout layout;
                      if (!LayoutStore::deserialize(
                              settings.value(QStringLiteral("layouts/") + key).toString(), &layout)) {
                          QMessageBox::warning(this, tr("Apply layout"),
                                               tr("That layout could not be read."));
                          return;
                      }
                      const bool twoPanels = layout.panelCount == 2;
                      applyWorkspace(twoPanels
                                         ? (layout.folderTree ? Workspace::Smidja
                                                              : Workspace::Tviskipt)
                                         : (layout.folderTree ? Workspace::Askr
                                                              : Workspace::Einn));
                      if (m_folderTreeDock)
                          m_folderTreeDock->setVisible(layout.folderTree);
                      if (m_placesBox)
                          m_placesBox->setVisible(layout.places);
                      if (!layout.sourceUri.isEmpty())
                          m_source->setUri(layout.sourceUri);
                      if (twoPanels && !layout.destUri.isEmpty())
                          m_dest->setUri(layout.destUri);
                  }});

    // The four named layouts inherited from Listi. Each is a preset over the
    // same pieces: how many panels, whether the folder tree is open, whether
    // Places is shown. Einn and Askr keep the active panel and hide the other.
    for (Workspace workspace : {Workspace::Einn, Workspace::Askr, Workspace::Tviskipt,
                                Workspace::Smidja}) {
        registry.add({QStringLiteral("view.") + workspaceId(workspace),
                      workspaceMenuLabel(workspace), tr("View"),
                      [this, workspace](CommandContext &) { applyWorkspace(workspace); }});
    }

    registry.add({QStringLiteral("tab.new"), tr("New tab"), tr("Windows"),
                  [this](CommandContext &) { newTab(); }});

    // A folder can be opened beside the current one instead of replacing it.
    // With a file selected the folder that holds it opens, which is what
    // "open the containing folder" means in practice.
    registry.add({QStringLiteral("tab.openFolder"), tr("Open in new tab"), tr("Windows"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      const QModelIndex index = panel->view()->currentIndex();
                      QString target = panel->uri();
                      if (index.isValid()) {
                          const QString uri = index.data(DirModel::UriRole).toString();
                          target = index.data(DirModel::IsDirRole).toBool()
                              ? uri
                              : VfsUri::parentUri(uri);
                      }

                      newTab(target);
                      m_source->focusLister();
                  }});

    registry.add({QStringLiteral("tab.close"), tr("Close tab"), tr("Windows"),
                  [this](CommandContext &) { closeTab(m_tabWidget->currentIndex()); }});

    registry.add({QStringLiteral("tab.next"), tr("Next tab"), tr("Windows"),
                  [this](CommandContext &) {
                      if (!m_tabWidget || m_tabWidget->count() <= 1)
                          return;
                      int next = m_tabWidget->currentIndex() + 1;
                      if (next >= m_tabWidget->count())
                          next = 0;
                      m_tabWidget->setCurrentIndex(next);
                  }});

    registry.add({QStringLiteral("tab.prev"), tr("Previous tab"), tr("Windows"),
                  [this](CommandContext &) {
                      if (!m_tabWidget || m_tabWidget->count() <= 1)
                          return;
                      int prev = m_tabWidget->currentIndex() - 1;
                      if (prev < 0)
                          prev = m_tabWidget->count() - 1;
                      m_tabWidget->setCurrentIndex(prev);
                  }});

    registry.add({QStringLiteral("panel.next"), tr("Switch panel"), tr("Windows"),
                  [this](CommandContext &) {
                      Panel *next = inactivePanel();
                      setActivePanel(next);
                      next->focusLister();
                  }});

    registry.add({QStringLiteral("app.quit"), tr("Quit"), tr("Application"),
                  [](CommandContext &) { QApplication::quit(); }});
}

void MainWindow::registerExternalTools()
{
    CommandRegistry &registry = CommandRegistry::instance();
    registry.removePrefix(QStringLiteral("tools.external."));

    QSettings settings;
    const QStringList entries = settings.value(QStringLiteral("tools/external")).toStringList();
    const QList<ExternalTools::Tool> tools = ExternalTools::parseAll(entries);
    for (int i = 0; i < tools.size(); ++i) {
        const ExternalTools::Tool tool = tools.at(i);
        registry.add({QStringLiteral("tools.external.%1").arg(i), tool.name, tr("Tools"),
                      [this, tool](CommandContext &) {
                          Panel *panel = activePanel();
                          const QString dir = Vfs::isLocal(panel->uri())
                              ? Vfs::localPathOf(panel->uri())
                              : QDir::homePath();
                          QStringList args = tool.arguments;
                          for (QString &arg : args)
                              arg.replace(QStringLiteral("%d"), dir);
                          QProcess::startDetached(tool.program, args, dir);
                      }});
    }
}

void MainWindow::copyToOtherPanel(bool moveFiles)
{
    if (singlePaneWorkspace()) {
        statusBar()->showMessage(tr("Direct copy and move need a two-panel workspace."), 4000);
        return;
    }

    Panel *from = activePanel();
    Panel *to = inactivePanel();

    const QModelIndexList selected = from->view()->selectionModel()->selectedIndexes();
    QVector<FileOps::CopyItem> items;
    items.reserve(selected.size());
    for (const QModelIndex &index : selected) {
        FileOps::CopyItem item;
        item.uri = index.data(DirModel::UriRole).toString();
        item.isDir = index.data(DirModel::IsDirRole).toBool();
        items.append(item);
    }

    runTransfer(items, to->uri(), moveFiles);
}

bool MainWindow::undoLast(QString *error)
{
    if (!m_undoStack.canUndo()) {
        if (error)
            *error = tr("Nothing to undo.");
        return false;
    }

    const UndoStack::Entry entry = m_undoStack.pop();

    switch (entry.type) {
    case UndoStack::OpType::Rename: {
        if (entry.sources.size() != 1 || entry.destinations.size() != 1) {
            if (error)
                *error = tr("Cannot undo this rename.");
            return false;
        }
        const QString src = entry.destinations.first();
        const QString dst = entry.sources.first();
        if (Vfs::isLocal(src) && Vfs::isLocal(dst)) {
            if (!QFile::rename(src, dst)) {
                if (error)
                    *error = tr("Could not undo rename.");
                return false;
            }
        } else {
            QString newUri;
            QString err;
            if (!Vfs::instance().rename(src, QFileInfo(dst).fileName(), &newUri, &err)) {
                if (error)
                    *error = err.isEmpty() ? tr("Could not undo rename.") : err;
                return false;
            }
        }
        return true;
    }
    case UndoStack::OpType::Mkdir: {
        for (const QString &dest : entry.destinations) {
            QString err;
            if (Vfs::isLocal(dest)) {
                if (!QDir().rmdir(dest) && QFileInfo(dest).exists()) {
                    if (error)
                        *error = tr("Could not remove the created folder (not empty?).");
                    return false;
                }
            } else {
                if (!Vfs::instance().remove(dest, false, &err)) {
                    if (error)
                        *error = err.isEmpty() ? tr("Could not remove the created folder.") : err;
                    return false;
                }
            }
        }
        return true;
    }
    case UndoStack::OpType::Trash: {
        if (entry.trashNames.isEmpty()) {
            if (error)
                *error = tr("Cannot undo this trash operation.");
            return false;
        }
        QString err;
        if (!FileOps::restoreFromTrash(entry.trashNames, &err)) {
            if (error)
                *error = err.isEmpty() ? tr("Could not restore from trash.") : err;
            return false;
        }
        return true;
    }
    case UndoStack::OpType::Copy: {
        for (int i = 0; i < entry.destinations.size(); ++i) {
            const QString dest = entry.destinations.at(i);
            const QString expected
                = i < entry.fingerprints.size() ? entry.fingerprints.at(i) : QString();
            QString err;
            if (Vfs::isLocal(dest)) {
                const QString path = Vfs::localPathOf(dest);
                if (!QFileInfo::exists(path) && !QFileInfo(path).isSymLink())
                    continue; // already gone; nothing to remove

                // Never delete a target that changed after the copy finished:
                // it is no longer (only) the file this job created.
                if (!expected.isEmpty() && TreeSnapshot::fingerprint(path) != expected) {
                    if (error)
                        *error = tr("%1 has changed since it was copied; it was not removed.")
                                     .arg(QFileInfo(path).fileName());
                    return false;
                }

                if (!FileOps::removePermanently({path}, &err)) {
                    if (error)
                        *error = err.isEmpty() ? tr("Could not remove %1.").arg(path) : err;
                    return false;
                }
            } else {
                if (!Vfs::instance().remove(dest, true, &err)) {
                    if (error)
                        *error = err.isEmpty() ? tr("Could not remove %1.").arg(dest) : err;
                    return false;
                }
            }
        }
        return true;
    }
    case UndoStack::OpType::Move: {
        // Move back: destination -> source.
        for (int i = 0; i < entry.destinations.size() && i < entry.sources.size(); ++i) {
            const QString src = entry.destinations.at(i);
            const QString dst = entry.sources.at(i);
            if (Vfs::isLocal(src) && Vfs::isLocal(dst)) {
                const QString path = Vfs::localPathOf(src);
                const QString expected
                    = i < entry.fingerprints.size() ? entry.fingerprints.at(i) : QString();
                if (!expected.isEmpty() && QFileInfo::exists(path)
                    && TreeSnapshot::fingerprint(path) != expected) {
                    if (error)
                        *error = tr("%1 has changed since it was moved; it was not moved back.")
                                     .arg(QFileInfo(path).fileName());
                    return false;
                }
                if (!QFileInfo::exists(path) && !QFileInfo(path).isSymLink()) {
                    if (error)
                        *error = tr("%1 is no longer there.").arg(QFileInfo(path).fileName());
                    return false;
                }
                if (!QFile::rename(path, dst)) {
                    if (error)
                        *error = tr("Could not move %1 back.").arg(path);
                    return false;
                }
            } else {
                if (error)
                    *error = tr("Undo of remote move is not supported.");
                return false;
            }
        }
        return true;
    }
    case UndoStack::OpType::Upload: {
        // Normally handled on a worker by edit.undo; the synchronous path is
        // kept correct for any other caller.
        QString err;
        const FileOps::Result result
            = FileOps::undoUploadsResult(entry.uploads, &err, FileOps::Progress());
        if (!result.failed.isEmpty() || !err.isEmpty()) {
            if (error)
                *error = err.isEmpty() ? tr("Could not undo the upload.") : err;
            return false;
        }
        return true;
    }
    case UndoStack::OpType::MoveOut: {
        QString err;
        const FileOps::Result result
            = FileOps::undoMovesOutResult(entry.movesOut, &err, FileOps::Progress());
        if (!result.failed.isEmpty() || !err.isEmpty()) {
            if (error)
                *error = err.isEmpty() ? tr("Could not undo the move.") : err;
            return false;
        }
        return true;
    }
    case UndoStack::OpType::RemoteMove: {
        QString err;
        const FileOps::Result result
            = FileOps::undoRemoteMovesResult(entry.movesRemote, &err, FileOps::Progress());
        if (!result.failed.isEmpty() || !err.isEmpty()) {
            if (error)
                *error = err.isEmpty() ? tr("Could not undo the move.") : err;
            return false;
        }
        return true;
    }
    }

    if (error)
        *error = tr("Nothing to undo.");
    return false;
}

void MainWindow::runTransfer(const QVector<FileOps::CopyItem> &items, const QString &targetUri,
                             bool moveFiles)
{
    if (items.isEmpty()) {
        statusBar()->showMessage(tr("Select something first, or drag the files over."), 4000);
        return;
    }

    if (!Vfs::instance().writable(targetUri)) {
        QMessageBox::information(this, tr("Read-only"),
                                 tr("Cannot write to %1 yet. Choose a writable folder.")
                                     .arg(Panel::displayFor(targetUri)));
        return;
    }

    const int count = int(items.size());
    // A move really moves a local source into an archive or a remote folder
    // (after the upload is verified), and between two archives or servers
    // (after a verified download and a verified upload). A source that lives
    // inside an archive or on a server and is copied into a local folder is
    // handled by the local move path instead.
    bool allSourcesLocal = true;
    bool allSourcesRemote = true;
    for (const FileOps::CopyItem &item : items) {
        const bool local = Vfs::isLocal(item.uri);
        allSourcesLocal = allSourcesLocal && local;
        allSourcesRemote = allSourcesRemote && !local;
    }
    // Between two archives or servers the entries are staged locally, checked
    // against their source listing, uploaded with a read-back check and only
    // then removed, so this is a real move and not a copy.
    const bool remoteToRemoteMove = moveFiles && !Vfs::isLocal(targetUri) && allSourcesRemote;
    const bool reallyMoves = moveFiles && (Vfs::isLocal(targetUri) || allSourcesLocal
                                           || remoteToRemoteMove);
    const QString title = reallyMoves
        ? tr("Moving %1 to %2").arg(itemCount(count), Panel::displayFor(targetUri))
        : tr("Copying %1 to %2").arg(itemCount(count), Panel::displayFor(targetUri));

    // The undo journal is written from the outcome of the job, and only for
    // destinations that did not exist before. Restoring something that was
    // replaced or never finished is what made Undo delete unrelated data.
    QVector<QPair<QString, QString>> created;
    if (Vfs::isLocal(targetUri)) {
        for (const FileOps::CopyItem &item : items) {
            // A source inside an archive or on a server is undone by MoveOut,
            // which can put the member back where it came from.
            if (moveFiles && !Vfs::isLocal(item.uri))
                continue;
            const QString destinationUri = VfsUri::childUri(targetUri, vfsLeafName(item.uri));
            const QString destinationPath = Vfs::localPathOf(destinationUri);
            if (!QFileInfo::exists(destinationPath) && !QFileInfo(destinationPath).isSymLink())
                created.append(qMakePair(item.uri, destinationUri));
        }
    }

    auto skipped = std::make_shared<int>(0);
    auto movedOut = std::make_shared<QVector<UploadRecord::MoveOutItem>>();
    const auto completion = [this, created, reallyMoves, skipped,
                             movedOut](const TaskOutcome &outcome) {
        m_lastJobSkipped = *skipped;
        if (outcome.cancelled || !outcome.failed.isEmpty() || !outcome.error.isEmpty())
            return;

        UndoStack::Entry ue;
        ue.type = reallyMoves ? UndoStack::OpType::Move : UndoStack::OpType::Copy;
        for (const auto &pair : created) {
            const QString path = Vfs::localPathOf(pair.second);
            if (!QFileInfo::exists(path) && !QFileInfo(path).isSymLink())
                continue;
            ue.sources.append(pair.first);
            ue.destinations.append(pair.second);
            ue.fingerprints.append(TreeSnapshot::fingerprint(path));
        }
        if (!ue.destinations.isEmpty())
            m_undoStack.push(ue);

        if (!movedOut->isEmpty()) {
            UndoStack::Entry moveOut;
            moveOut.type = UndoStack::OpType::MoveOut;
            moveOut.movesOut = *movedOut;
            m_undoStack.push(moveOut);
        }
    };

    if (Vfs::isLocal(targetUri)) {
        const QString targetDir = Vfs::localPathOf(targetUri);
        // The conflict answer belongs to this job, so a queued job never
        // inherits "Replace All" from the one before it.
        auto conflictMode = std::make_shared<int>(0);
        startTask(title, [this, items, targetDir, moveFiles, conflictMode, skipped,
                          movedOut](FileOps::Progress &progress, QString *error) {
            progress.conflict = [this, conflictMode](const QString &source, const QString &target) {
                int result = FileOps::ConflictSkip;
                QMetaObject::invokeMethod(this, [this, &result, source, target, conflictMode]() {
                    result = resolveConflict(source, target, conflictMode.get());
                }, Qt::BlockingQueuedConnection);
                return result;
            };
            const FileOps::Result result
                = FileOps::copyUrisResult(items, targetDir, moveFiles, error, progress);
            *skipped = result.itemsSkipped;
            *movedOut = result.movesOut;
            return result.failed;
        }, QString(), completion);
        return;
    }

    if (remoteToRemoteMove) {
        // Moving between two archives or servers: stage, verify, upload with a
        // read-back check, and remove the original only after that.
        auto remoteConflictMode = std::make_shared<int>(0);
        auto movedRemote = std::make_shared<QVector<UploadRecord::RemoteMoveItem>>();
        startTask(title, [this, items, targetUri, skipped, remoteConflictMode,
                          movedRemote](FileOps::Progress &progress, QString *error) {
            progress.conflict = [this, remoteConflictMode](const QString &source,
                                                            const QString &target) {
                int result = FileOps::ConflictSkip;
                QMetaObject::invokeMethod(this, [this, &result, source, target, remoteConflictMode]() {
                    result = resolveConflict(source, target, remoteConflictMode.get());
                }, Qt::BlockingQueuedConnection);
                return result;
            };
            const FileOps::Result result
                = FileOps::moveRemoteToRemoteResult(items, targetUri, error, progress);
            *skipped = result.itemsSkipped;
            *movedRemote = result.movesRemote;
            return result.failed;
        }, QString(), [this, movedRemote](const TaskOutcome &outcome) {
            if (outcome.cancelled || !outcome.failed.isEmpty() || !outcome.error.isEmpty()
                || movedRemote->isEmpty()) {
                return;
            }
            UndoStack::Entry undo;
            undo.type = UndoStack::OpType::RemoteMove;
            undo.movesRemote = *movedRemote;
            m_undoStack.push(undo);
        });
        return;
    }

    // Writing into an archive or remote folder: upload each item through the
    // VFS. Every uploaded file is read back and compared with its source, and a
    // local source is removed only after that verification.
    auto conflictMode = std::make_shared<int>(0);
    auto uploadNotes = std::make_shared<QStringList>();
    auto uploaded = std::make_shared<QVector<UploadRecord::Item>>();
    startTask(title, [this, items, targetUri, moveFiles, skipped, conflictMode, uploadNotes,
                      uploaded](FileOps::Progress &progress, QString *error) {
        progress.conflict = [this, conflictMode](const QString &source, const QString &target) {
            int result = FileOps::ConflictSkip;
            QMetaObject::invokeMethod(this, [this, &result, source, target, conflictMode]() {
                result = resolveConflict(source, target, conflictMode.get());
            }, Qt::BlockingQueuedConnection);
            return result;
        };
        const FileOps::Result result
            = FileOps::uploadUrisResult(items, targetUri, moveFiles, error, progress);
        *skipped = result.itemsSkipped;
        *uploadNotes = result.notes;
        *uploaded = result.uploads;
        return result.failed;
    }, QString(), [this, completion, uploadNotes, uploaded](const TaskOutcome &outcome) {
        completion(outcome);
        if (!outcome.cancelled && outcome.failed.isEmpty() && outcome.error.isEmpty()
            && !uploaded->isEmpty()) {
            UndoStack::Entry undo;
            undo.type = UndoStack::OpType::Upload;
            undo.uploads = *uploaded;
            m_undoStack.push(undo);
        }
        if (!uploadNotes->isEmpty())
            QMessageBox::information(this, tr("Upload"), uploadNotes->join(QLatin1Char('\n')));
    });
}

int MainWindow::resolveConflict(const QString &source, const QString &target, int *mode)
{
    const int current = mode ? *mode : 0;
    if (current == 1)
        return FileOps::ConflictReplace;
    if (current == 2)
        return FileOps::ConflictSkip;
    if (current == 3)
        return FileOps::ConflictKeepBoth;

    QMessageBox box(this);
    box.setWindowTitle(tr("File already exists"));
    box.setText(tr("\"%1\" already exists at the destination.").arg(QFileInfo(target).fileName()));
    box.setInformativeText(tr("Source: %1\nDestination: %2").arg(source, target));
    QAbstractButton *replaceBtn = box.addButton(tr("Replace"), QMessageBox::AcceptRole);
    QAbstractButton *skipBtn = box.addButton(tr("Skip"), QMessageBox::RejectRole);
    QAbstractButton *keepBothBtn = box.addButton(tr("Keep both"), QMessageBox::AcceptRole);
    QAbstractButton *replaceAllBtn = box.addButton(tr("Replace All"), QMessageBox::AcceptRole);
    QAbstractButton *skipAllBtn = box.addButton(tr("Skip All"), QMessageBox::RejectRole);
    QAbstractButton *keepBothAllBtn = box.addButton(tr("Keep both All"), QMessageBox::AcceptRole);
    QAbstractButton *abortBtn = box.addButton(tr("Abort"), QMessageBox::DestructiveRole);
    box.setDefaultButton(qobject_cast<QPushButton *>(skipBtn));
    box.exec();
    QAbstractButton *clicked = box.clickedButton();
    if (clicked == replaceBtn)
        return FileOps::ConflictReplace;
    if (clicked == skipBtn)
        return FileOps::ConflictSkip;
    if (clicked == replaceAllBtn) {
        if (mode)
            *mode = 1;
        return FileOps::ConflictReplace;
    }
    if (clicked == skipAllBtn) {
        if (mode)
            *mode = 2;
        return FileOps::ConflictSkip;
    }
    if (clicked == keepBothBtn)
        return FileOps::ConflictKeepBoth;
    if (clicked == keepBothAllBtn) {
        if (mode)
            *mode = 3;
        return FileOps::ConflictKeepBoth;
    }
    return FileOps::ConflictAbort;
}

bool MainWindow::startTask(const QString &title, const FileTask::Job &job,
                           const QString &jobId,
                           const std::function<void(const TaskOutcome &)> &onDone)
{
    if (m_task) {
        const QString queuedId = m_jobReports.enqueue(title);
        m_pendingTasks.enqueue(PendingTask{title, job, queuedId, onDone});
        refreshOperationsView();
        statusBar()->showMessage(tr("“%1” queued — %2 waiting.")
                                     .arg(title)
                                     .arg(m_pendingTasks.size()),
                                 5000);
        return false;
    }

    const QString activeId = jobId.isEmpty() ? m_jobReports.enqueue(title) : jobId;
    m_jobReports.start(activeId);
    m_currentJobId = activeId;
    m_currentCompletion = onDone;
    m_lastJobSkipped = 0;
    refreshOperationsView();

    m_task = FileTask::start(title, job, this);

    auto *dialog = new OperationDialog(title, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    m_operation = dialog;

    connect(m_task, &FileTask::progress, dialog,
            [this](qint64 bytes, qint64 total, const QString &current, int items) {
                if (m_operation)
                    m_operation->showProgress(bytes, total, current, items);
                if (!m_currentJobId.isEmpty())
                    m_jobReports.progress(m_currentJobId, bytes, total, items, 0);
                scheduleOperationsRefresh();
            });
    connect(dialog, &OperationDialog::cancelRequested, this, [this]() {
        if (m_task)
            m_task->cancel();
    });
    connect(dialog, &OperationDialog::pauseRequested, this, [this](bool paused) {
        if (!m_task)
            return;
        if (paused)
            m_task->pause();
        else
            m_task->resume();
        if (m_operation)
            m_operation->setPaused(paused);
    });
    connect(m_task, &FileTask::finished, this, &MainWindow::taskFinished);

    statusBar()->showMessage(title);
    dialog->showProgress(0, 0, QString(), 0);
    dialog->show();
    return true;
}

void MainWindow::startNextTask()
{
    if (m_task || m_pendingTasks.isEmpty())
        return;

    const PendingTask next = m_pendingTasks.dequeue();
    startTask(next.title, next.job, next.jobId, next.onDone);
}

void MainWindow::taskFinished(const TaskOutcome &outcome)
{
    FileTask *task = m_task;
    m_task = nullptr;
    Q_UNUSED(task)

    if (m_operation) {
        QPointer<OperationDialog> dialog = m_operation;
        m_operation = nullptr;
        dialog->close();
    }

    refreshBoth();

    if (!m_currentJobId.isEmpty()) {
        m_jobReports.progress(m_currentJobId, outcome.bytesTransferred,
                             outcome.bytesTotal, outcome.transferred,
                             outcome.transferred);
        if (outcome.cancelled)
            m_jobReports.cancel(m_currentJobId);
        else if (!outcome.failed.isEmpty() || !outcome.error.isEmpty())
            m_jobReports.fail(m_currentJobId,
                             outcome.error.isEmpty() ? tr("Transfer failed") : outcome.error);
        else
            m_jobReports.complete(m_currentJobId);
        m_currentJobId.clear();
    }
    if (m_operationsRefreshTimer)
        m_operationsRefreshTimer->stop();
    refreshOperationsView();

    // The job's completion hook decides whether an undo entry may be written.
    const auto completion = m_currentCompletion;
    m_currentCompletion = nullptr;
    if (completion)
        completion(outcome);

    if (outcome.cancelled) {
        statusBar()->showMessage(tr("Cancelled after %1.").arg(itemCount(outcome.transferred)),
                                 6000);
        startNextTask();
        return;
    }

    if (!outcome.failed.isEmpty() || !outcome.error.isEmpty()) {
        QMessageBox::warning(this, tr("The operation stopped"),
                             outcome.error.isEmpty()
                                 ? tr("%1 could not be transferred.")
                                       .arg(itemCount(int(outcome.failed.size())))
                                 : outcome.error);
        startNextTask();
        return;
    }

    if (m_lastJobSkipped > 0) {
        statusBar()->showMessage(tr("Done — %1, %2 skipped.")
                                     .arg(itemCount(outcome.transferred))
                                     .arg(m_lastJobSkipped),
                                 6000);
    } else {
        statusBar()->showMessage(tr("Done — %1.").arg(itemCount(outcome.transferred)), 5000);
    }
    startNextTask();
}

void MainWindow::buildMenus()
{
    // Rebuild from scratch so dynamically registered commands (e.g. external
    // tools edited at runtime) replace the previous menu entries instead of
    // duplicating them.
    menuBar()->clear();

    // Menus are generated from the command registry, grouped by category, so a
    // new command shows up in the menu, the palette and the shortcut sheet at
    // once.
    static const QStringList order = {
        QStringLiteral("Navigation"), QStringLiteral("View"), QStringLiteral("Selection"),
        QStringLiteral("Files"),      QStringLiteral("Windows"), QStringLiteral("Application"),
    };

    const CommandRegistry &registry = CommandRegistry::instance();
    QStringList categories = order;
    for (const Command &command : registry.all()) {
        if (!command.category.isEmpty() && !categories.contains(command.category))
            categories.append(command.category);
    }

    for (const QString &category : categories) {
        if (category == QLatin1String("Help"))
            continue;

        QMenu *menu = menuBar()->addMenu(category);
        for (const Command &command : registry.all()) {
            if (command.category != category)
                continue;

            QAction *action = menu->addAction(command.title);
            const QString shortcut = registry.shortcutFor(command.id);
            if (!shortcut.isEmpty())
                action->setToolTip(shortcut);
            connect(action, &QAction::triggered, this, [this, id = command.id]() {
                CommandContext context;
                context.sourcePath = m_source->uri();
                context.destPath = m_dest->uri();
                context.selection = activePanel()->selection();
                CommandRegistry::instance().run(id, context);
            });
        }
    }

    QMenu *help = menuBar()->addMenu(tr("Help"));
    for (const Command &command : registry.all()) {
        if (command.category != QLatin1String("Help"))
            continue;
        QAction *action = help->addAction(command.title);
        connect(action, &QAction::triggered, this, [this, id = command.id]() {
            CommandContext context;
            CommandRegistry::instance().run(id, context);
        });
    }
}

void MainWindow::buildPlaces()
{
    struct Place
    {
        QString label;
        QString themeIcon;
        QStyle::StandardPixmap fallback;
        QString path;
    };

    QVector<Place> places;
    const auto addStandard = [&places](const QString &label, const QString &themeIcon,
                                       QStyle::StandardPixmap fallback, const QString &path) {
        if (path.isEmpty() || !QFileInfo(path).isDir())
            return;
        places.append({label, themeIcon, fallback, path});
    };

    addStandard(tr("Home"), QStringLiteral("go-home"), QStyle::SP_DirHomeIcon, QDir::homePath());
    addStandard(tr("Documents"), QStringLiteral("folder-documents"), QStyle::SP_DirIcon,
                QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    addStandard(tr("Downloads"), QStringLiteral("folder-download"), QStyle::SP_DirIcon,
                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    addStandard(tr("Pictures"), QStringLiteral("folder-pictures"), QStyle::SP_DirIcon,
                QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    addStandard(tr("Music"), QStringLiteral("folder-music"), QStyle::SP_DirIcon,
                QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    addStandard(tr("Videos"), QStringLiteral("folder-videos"), QStyle::SP_DirIcon,
                QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
    addStandard(tr("Trash"), QStringLiteral("user-trash"), QStyle::SP_TrashIcon,
                QDir::homePath() + QStringLiteral("/.local/share/Trash/files"));

    // Only volumes a person would recognise as a place. Pseudo filesystems and
    // container mounts are noise in a sidebar.
    const auto isUserVolume = [](const QStorageInfo &volume) {
        static const QStringList skipFileSystems = {
            QStringLiteral("tmpfs"),     QStringLiteral("devtmpfs"), QStringLiteral("squashfs"),
            QStringLiteral("overlay"),   QStringLiteral("proc"),     QStringLiteral("sysfs"),
            QStringLiteral("cgroup2"),   QStringLiteral("cgroup"),   QStringLiteral("devpts"),
            QStringLiteral("ramfs"),     QStringLiteral("autofs"),   QStringLiteral("nsfs"),
        };
        if (skipFileSystems.contains(QString::fromUtf8(volume.fileSystemType())))
            return false;

        static const QStringList skipPaths = {
            QStringLiteral("/run"),     QStringLiteral("/sys"),
            QStringLiteral("/proc"),    QStringLiteral("/dev"),
            QStringLiteral("/snap"),    QStringLiteral("/boot/efi"),
            QStringLiteral("/var/lib/docker"),
        };
        const QString root = volume.rootPath();
        for (const QString &skip : skipPaths) {
            if (root == skip || root.startsWith(skip + QLatin1Char('/')))
                return false;
        }

        return volume.bytesTotal() >= 100LL * 1024 * 1024;
    };

    QSet<QString> seenPaths;
    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &volume : volumes) {
        if (!volume.isValid() || !volume.isReady() || !isUserVolume(volume))
            continue;
        if (seenPaths.contains(volume.rootPath()))
            continue;
        seenPaths.insert(volume.rootPath());

        const QString name = volume.displayName().isEmpty() ? volume.rootPath()
                                                           : volume.displayName();
        addStandard(name, QStringLiteral("drive-harddisk"), QStyle::SP_DriveHDIcon,
                    volume.rootPath());
    }

    QStringList bookmarks = QSettings().value(QStringLiteral("ui/bookmarks")).toStringList();
    for (const QString &bookmark : bookmarks) {
        const QString label = vfsLeafName(bookmark).isEmpty() ? bookmark : vfsLeafName(bookmark);
        places.append({label, QStringLiteral("bookmark-new"), QStyle::SP_DirLinkIcon, bookmark});
    }

    m_places->clear();
    for (const Place &place : places) {
        QIcon icon = QIcon::fromTheme(place.themeIcon);
        if (icon.isNull())
            icon = style()->standardIcon(place.fallback);

        auto *item = new QListWidgetItem(icon, place.label, m_places);
        const bool isUri = place.path.contains(QStringLiteral("://"));
        item->setData(Qt::UserRole, isUri ? place.path
                                         : QUrl::fromLocalFile(place.path).toString());
        item->setToolTip(place.path);
    }

    connect(m_places, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item)
            return;
        activePanel()->setUri(item->data(Qt::UserRole).toString());
        activePanel()->focusLister();
    });
}

void MainWindow::applyWorkspace(Workspace workspace)
{
    m_workspace = workspace;

    const bool twoPanels = !singlePaneWorkspace();
    const bool tree = workspace == Workspace::Askr || workspace == Workspace::Smidja;
    // Tvískipt is the two-panel copying desk: the Places column would take
    // width from the panels by default, so it starts hidden there.
    const bool places = workspace == Workspace::Askr || workspace == Workspace::Smidja;

    if (m_folderTreeDock)
        m_folderTreeDock->setVisible(tree);

    if (m_placesBox)
        m_placesBox->setVisible(places);

    // Smiðja is the workbench with a preview under each panel. The other
    // workspaces keep the file lists tall, as Directory Opus does.
    const bool preview = workspace == Workspace::Smidja;
    for (const Tab &tab : m_tabs) {
        if (tab.source)
            tab.source->setPreviewVisible(preview);
        if (tab.dest)
            tab.dest->setPreviewVisible(preview);
    }

    // Einn and Askr show the active panel; the other one stays alive so its
    // location and selection are still there when a two-panel workspace returns.
    if (!twoPanels && m_active != m_source && m_active != m_dest)
        setActivePanel(m_source);
    updatePanelVisibility();

    persistWorkspace();

    statusBar()->showMessage(tr("%1 workspace.").arg(workspaceName(workspace)), 2500);
}

void MainWindow::updatePanelVisibility()
{
    if (!m_source || !m_dest)
        return;

    const bool twoPanels = !singlePaneWorkspace();
    if (twoPanels) {
        m_source->setVisible(true);
        m_dest->setVisible(true);
        return;
    }

    // Keep the panel the user was working in visible. If the active pointer is
    // stale, fall back to the source panel rather than hiding both.
    if (m_active != m_source && m_active != m_dest)
        m_active = m_source;
    m_source->setVisible(m_active == m_source);
    m_dest->setVisible(m_active == m_dest);
}

bool MainWindow::singlePaneWorkspace() const
{
    return m_workspace == Workspace::Einn || m_workspace == Workspace::Askr;
}

Workspace MainWindow::loadWorkspace() const
{
    const QSettings settings;

    // Numeric ids are the stable on-disk form; the ASCII id is easier to read
    // and is what a newer build writes beside it.
    if (settings.contains(QStringLiteral("workspace/current"))) {
        const int stored = settings.value(QStringLiteral("workspace/current")).toInt();
        if (stored >= 0 && stored <= 3)
            return static_cast<Workspace>(stored);
    }

    if (settings.contains(QStringLiteral("workspace/id"))) {
        const QString id = settings.value(QStringLiteral("workspace/id")).toString();
        if (id == QStringLiteral("einn"))
            return Workspace::Einn;
        if (id == QStringLiteral("askr"))
            return Workspace::Askr;
        if (id == QStringLiteral("smidja"))
            return Workspace::Smidja;
        if (id == QStringLiteral("tviskipt"))
            return Workspace::Tviskipt;
    }

    // The pre-workspace configuration only had a single-panel flag.
    return settings.value(QStringLiteral("ui/singlePanel"), false).toBool()
        ? Workspace::Einn
        : Workspace::Tviskipt;
}

void MainWindow::persistWorkspace()
{
    QSettings settings;
    settings.setValue(QStringLiteral("workspace/current"), static_cast<int>(m_workspace));
    settings.setValue(QStringLiteral("workspace/id"), workspaceId(m_workspace));
    // Keep the old flag in sync so a rollback to the previous build still opens
    // the same number of panels.
    settings.setValue(QStringLiteral("ui/singlePanel"), singlePaneWorkspace());
}

void MainWindow::togglePlaces()
{
    if (!m_placesBox)
        return;
    m_placesBox->setVisible(!m_placesBox->isVisible());
}

void MainWindow::refreshShelfView()
{
    if (!m_shelfList)
        return;
    m_shelfList->clear();
    for (const Shelf::Entry &entry : m_shelf.entries()) {
        auto *item = new QListWidgetItem(entry.name, m_shelfList);
        item->setData(Qt::UserRole, entry.uri);
        item->setData(Qt::UserRole + 1, entry.isDir);
        item->setToolTip(entry.uri);
    }
}

void MainWindow::refreshOperationsView()
{
    if (!m_operationsDock || !m_operationsDock->isVisible())
        return;
    if (!m_operationsList)
        return;
    // A direct rebuild supersedes any throttled refresh that is still pending.
    if (m_operationsRefreshTimer)
        m_operationsRefreshTimer->stop();
    m_operationsList->clear();
    for (const JobReport::Report &report : m_jobReports.all()) {
        QString state;
        switch (report.state) {
        case JobReport::State::Waiting:
            state = tr("waiting");
            break;
        case JobReport::State::Running:
            state = tr("running");
            break;
        case JobReport::State::Completed:
            state = tr("completed");
            break;
        case JobReport::State::Failed:
            state = tr("failed");
            break;
        case JobReport::State::Cancelled:
            state = tr("cancelled");
            break;
        }
        QString text = tr("%1 — %2").arg(report.title, state);
        if (report.itemsTotal > 0)
            text += tr(" (%1/%2)").arg(report.itemsDone).arg(report.itemsTotal);
        if (!report.error.isEmpty())
            text += QStringLiteral(" — ") + report.error;
        auto *item = new QListWidgetItem(text, m_operationsList);
        item->setData(Qt::UserRole, report.id);
    }
}

void MainWindow::scheduleOperationsRefresh()
{
    if (!m_operationsDock || !m_operationsDock->isVisible())
        return;
    if (!m_operationsRefreshTimer || m_operationsRefreshTimer->isActive())
        return;
    m_operationsRefreshTimer->start(250);
}

void MainWindow::cancelJob(const QString &id)
{
    if (id.isEmpty())
        return;
    if (id == m_currentJobId && m_task) {
        m_task->cancel();
        return;
    }
    for (int i = 0; i < m_pendingTasks.size(); ++i) {
        if (m_pendingTasks.at(i).jobId != id)
            continue;
        m_pendingTasks.removeAt(i);
        m_jobReports.cancel(id);
        refreshOperationsView();
        statusBar()->showMessage(tr("Cancelled queued job."), 2500);
        return;
    }
    // Already finished or unknown: nothing to cancel.
    refreshOperationsView();
}

void MainWindow::showWelcome()
{
    QSettings().setValue(QStringLiteral("ui/welcomeShown"), true);

    QMessageBox box(this);
    box.setWindowTitle(tr("Welcome to Listista"));
    box.setTextFormat(Qt::RichText);
    box.setText(tr("<b>Two panels, one workflow.</b>"));
    box.setInformativeText(
        tr("<p>The left panel is your source, the right one is the target. <b>Tab</b> "
           "switches between them, and the active panel has a coloured frame.</p>"
           "<p><b>Enter</b> opens a folder — and a zip, tar or 7z is a folder too. "
           "<b>Backspace</b> takes you up one level.</p>"
           "<p><b>Alt+C</b> copies and <b>Alt+M</b> moves what you have selected to the "
           "other panel. You can drag the files across as well.</p>"
           "<p>Heading for FTP, SFTP, SMB or an archive? Press <b>Ctrl+L</b> and type the "
           "address, for example <i>ftp://host/folder</i>.</p>"
           "<p>Everything the program can do is a command: press <b>Ctrl+K</b> and type "
           "what you want to do. <b>Ctrl+Shift+K</b> lists every shortcut.</p>"
           "<p><i>A proud member of the Listi fan club.</i></p>"));
    box.setStandardButtons(QMessageBox::Ok);
    box.button(QMessageBox::Ok)->setText(tr("Get started"));
    box.exec();
}

void MainWindow::showAbout()
{
    QMessageBox box(this);
    box.setWindowTitle(tr("About Listista"));
    box.setIcon(QMessageBox::Information);
    box.setTextFormat(Qt::RichText);
    box.setTextInteractionFlags(Qt::TextBrowserInteraction);
    box.setText(tr("<h2>Listista</h2>"
                   "<p><i>A proud member of the Listi fan club.</i></p>"));
    box.setInformativeText(
        tr("<p>Listista is a fan-made, independently developed adaptation of "
           "<a href=\"https://github.com/kambuziarendal/listi\">Listi</a> for Haiku. "
           "It brings Listi's familiar interface to other operating systems while "
           "following its own release schedule.</p>"
           "<p>Listi on Haiku remains the original. A Listista is a fan of Listi, and "
           "this adaptation is developed independently, with its own version numbers "
           "and releases, and may implement ideas from Listi's roadmap at its own pace.</p>"
           "<p>Version %1 · MIT License<br>"
           "Copyright © 2026 joddsus</p>"
           "<p>Listista is MIT licensed. Listi is Copyright © 2026 Kambuzi EDB "
           "and is also MIT licensed.</p>"
           "<p><a href=\"https://github.com/joddsus/listilin\">Project repository</a></p>")
            .arg(QCoreApplication::applicationVersion()));
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void MainWindow::showContextMenu(Panel *panel, const QPoint &globalPos, bool fileMenu)
{
    QMenu *menu = createContextMenu(panel, fileMenu);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->exec(globalPos);
}

QMenu *MainWindow::createContextMenu(Panel *panel, bool fileMenu)
{
    auto *menu = new QMenu(this);
    CommandRegistry &registry = CommandRegistry::instance();

    // The switch sits at the top of both menus. Right-clicking a file and then
    // wanting "New folder" should cost one click, not a trip to the menu bar.
    const bool hasCurrent = panel->view()->currentIndex().isValid();
    if (fileMenu || hasCurrent) {
        // Worded as an action, not as a submenu: it swaps this menu for the other.
        QAction *flip = menu->addAction(fileMenu ? tr("Show folder menu")
                                                : tr("Show file menu"));
        flip->setStatusTip(fileMenu ? tr("Commands for the folder itself")
                                    : tr("Commands for the selected item"));
        connect(flip, &QAction::triggered, this, [this, flip, panel, fileMenu]() {
            // Reopen where this menu was, once it has closed, so the platform does
            // not hand the pending mouse release to the new menu. The menu's own
            // position beats the cursor: the switch also works from the keyboard.
            QPoint at = QCursor::pos();
            if (auto *owner = qobject_cast<QMenu *>(flip->parent()))
                at = owner->pos();

            QTimer::singleShot(0, this, [this, panel, at, fileMenu]() {
                showContextMenu(panel, at, !fileMenu);
            });
        });
        menu->addSeparator();
    }

    const QStringList &ids = fileMenu ? fileContextMenu() : folderContextMenu();
    for (const QString &commandId : ids) {
        if (commandId == QLatin1String("-")) {
            menu->addSeparator();
            continue;
        }

        const Command *command = registry.find(commandId);
        if (!command)
            continue;

        QString label = command->title;
        const QString shortcut = registry.shortcutFor(commandId);
        if (!shortcut.isEmpty())
            label += QLatin1Char('\t') + shortcut;

        QAction *action = menu->addAction(label);
        connect(action, &QAction::triggered, this, [this, commandId]() {
            CommandContext context;
            context.sourcePath = m_source->uri();
            context.destPath = m_dest->uri();
            context.selection = activePanel()->selection();
            CommandRegistry::instance().run(commandId, context);
        });
    }

    return menu;
}

void MainWindow::restoreSession(const QString &startPath)
{
    const QSettings settings;

    QString source = startPath;
    if (source.isEmpty())
        source = VfsUri::redactCredentials(settings.value(QStringLiteral("startup/source"), QDir::homePath()).toString());
    if (!QFileInfo(Vfs::localPathOf(source)).isDir() && !VfsUri::parse(source).isRemote()
        && !VfsUri::parse(source).isArchive()) {
        source = QDir::homePath();
    }

    QString dest = VfsUri::redactCredentials(settings.value(QStringLiteral("startup/dest"), QDir::homePath()).toString());
    if (!QFileInfo(Vfs::localPathOf(dest)).isDir() && !VfsUri::parse(dest).isRemote()
        && !VfsUri::parse(dest).isArchive()) {
        dest = QDir::homePath();
    }

    const bool showHidden = settings.value(QStringLiteral("view/showHidden"), true).toBool();
    m_source->model()->setShowHidden(showHidden);
    m_dest->model()->setShowHidden(showHidden);

    m_source->setUri(source);
    m_dest->setUri(dest);

    applyWorkspace(loadWorkspace());

    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
}

void MainWindow::saveSession()
{
    persistWorkspace();

    // Persist through the validated, atomic writer so a crash or tampered
    // settings file cannot leave a half-written configuration behind.
    QMap<QString, QVariant> updates;
    updates.insert(QStringLiteral("startup/source"),
                   VfsUri::redactCredentials(m_source->uri()));
    updates.insert(QStringLiteral("startup/dest"),
                   VfsUri::redactCredentials(m_dest->uri()));
    updates.insert(QStringLiteral("view/showHidden"), m_source->model()->showHidden());
    updates.insert(QStringLiteral("window/geometry"), saveGeometry());

    QString error;
    QSettings settings;
    if (!SettingsSafety::updateSettings(settings.fileName(), updates, &error))
        qWarning("Listista: could not save settings: %s", qPrintable(error));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_operationsRefreshTimer)
        m_operationsRefreshTimer->stop();
    saveSession();
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_shelfList
        || (m_shelfList && watched == m_shelfList->viewport())) {
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *drag = static_cast<QDragMoveEvent *>(event);
            const QMimeData *mime = drag->mimeData();
            if (mime && (mime->hasUrls()
                         || mime->hasFormat(QStringLiteral("application/x-lister-items")))) {
                drag->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto *drop = static_cast<QDropEvent *>(event);
            const QList<Shelf::Entry> entries = Shelf::entriesFromMimeData(drop->mimeData());
            for (const Shelf::Entry &entry : entries)
                m_shelf.addUnique(entry);
            if (!entries.isEmpty()) {
                refreshShelfView();
                if (!m_shelfDock->isVisible())
                    m_shelfDock->show();
                statusBar()->showMessage(
                    tr("Shelf holds %1 entries.").arg(m_shelf.count()), 3000);
                drop->acceptProposedAction();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::openPreferences()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Preferences"));
    dialog.resize(720, 540);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Keys are global, except inside text fields. "
                                   "Clear a key to disable it."),
                                 &dialog));

    auto *table = new QTableWidget(&dialog);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({tr("Command"), tr("Category"), tr("Key")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->verticalHeader()->hide();
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    CommandRegistry &registry = CommandRegistry::instance();
    QVector<QKeySequenceEdit *> editors;
    QStringList original;
    for (const Command &command : registry.all()) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(command.title));
        table->setItem(row, 1, new QTableWidgetItem(command.category));
        const QString current = registry.shortcutsFor(command.id).value(0);
        auto *editor = new QKeySequenceEdit(table);
        editor->setKeySequence(QKeySequence(current));
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        editor->setMaximumSequenceLength(1);
#endif
        table->setCellWidget(row, 2, editor);
        editors.append(editor);
        original.append(current);
    }
    layout->addWidget(table, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    QSettings settings;
    for (int row = 0; row < editors.size(); ++row) {
        const QString id = registry.all().value(row).id;
        const QString updated = editors.value(row)->keySequence().toString(QKeySequence::PortableText);
        if (updated == original.value(row))
            continue;
        settings.setValue(QStringLiteral("keymap/") + id, updated);
    }
    loadBindings();
    statusBar()->showMessage(tr("Keys updated."), 3000);
}

void MainWindow::loadBindings()
{
    CommandRegistry &registry = CommandRegistry::instance();
    registry.clearBindings();
    qDeleteAll(m_globalShortcuts);
    m_globalShortcuts.clear();

    // No function keys anywhere: on a laptop they are volume, brightness and
    // media, and on a desktop they are simply out of reach. Every default sits
    // under the left hand instead.
    const QVector<QPair<QString, QString>> defaults = {
        {QStringLiteral("lister.open"), QStringLiteral("Return")},
        {QStringLiteral("lister.parent"), QStringLiteral("Backspace")},
        {QStringLiteral("lister.parent"), QStringLiteral("Alt+Up")},
        {QStringLiteral("lister.home"), QStringLiteral("Ctrl+H")},
        {QStringLiteral("lister.refresh"), QStringLiteral("Ctrl+R")},
        {QStringLiteral("lister.focusPath"), QStringLiteral("Ctrl+L")},
        {QStringLiteral("lister.historyBack"), QStringLiteral("Alt+Left")},
        {QStringLiteral("lister.historyForward"), QStringLiteral("Alt+Right")},
        {QStringLiteral("lister.toggleHidden"), QStringLiteral("Ctrl+Shift+H")},
        {QStringLiteral("view.icons"), QStringLiteral("Ctrl+I")},
        {QStringLiteral("view.filter"), QStringLiteral("Ctrl+F")},
        {QStringLiteral("tools.terminal"), QStringLiteral("Ctrl+T")},
        {QStringLiteral("tools.find"), QStringLiteral("Ctrl+Shift+F")},
        {QStringLiteral("bookmark.add"), QStringLiteral("Ctrl+D")},
        {QStringLiteral("selection.toggle"), QStringLiteral("Space")},
        {QStringLiteral("file.copy"), QStringLiteral("Alt+C")},
        {QStringLiteral("file.move"), QStringLiteral("Alt+M")},
        {QStringLiteral("file.mkdir"), QStringLiteral("Ctrl+Shift+N")},
        {QStringLiteral("file.rename"), QStringLiteral("Alt+R")},
        {QStringLiteral("file.trash"), QStringLiteral("Delete")},
        {QStringLiteral("file.properties"), QStringLiteral("Alt+Enter")},
        {QStringLiteral("file.view"), QStringLiteral("Alt+V")},
        {QStringLiteral("file.edit"), QStringLiteral("Alt+E")},
        {QStringLiteral("file.clipboardCopy"), QStringLiteral("Ctrl+C")},
        {QStringLiteral("file.clipboardCut"), QStringLiteral("Ctrl+X")},
        {QStringLiteral("file.clipboardPaste"), QStringLiteral("Ctrl+V")},
        {QStringLiteral("file.copyPath"), QStringLiteral("Ctrl+Shift+C")},
        {QStringLiteral("panel.next"), QStringLiteral("Tab")},
        {QStringLiteral("edit.undo"), QStringLiteral("Ctrl+Z")},
        {QStringLiteral("app.palette"), QStringLiteral("Ctrl+K")},
        {QStringLiteral("app.preferences"), QStringLiteral("Ctrl+,")},
        {QStringLiteral("help.shortcuts"), QStringLiteral("Ctrl+Shift+K")},
        {QStringLiteral("view.togglePlaces"), QStringLiteral("Alt+P")},
        {QStringLiteral("app.quit"), QStringLiteral("Ctrl+Q")},
    };

    QHash<QString, QStringList> byCommand;
    for (const auto &binding : defaults)
        byCommand[binding.first].append(binding.second);

    // User overrides from the config file: keymap/<command id> = sequence.
    // An empty value means “this command has no key”, which is how a default
    // can be removed without losing it from the built-in set.
    const QSettings settings;
    QHash<QString, QString> overrides;
    // Listi 0.31.1 renamed the workspace commands. Carry a keymap written by
    // the previous build over to the new ids instead of dropping the binding.
    static const QHash<QString, QString> legacyWorkspaceCommands = {
        {QStringLiteral("view.simple"), QStringLiteral("view.einn")},
        {QStringLiteral("view.explorer"), QStringLiteral("view.askr")},
        {QStringLiteral("view.commander"), QStringLiteral("view.tviskipt")},
        {QStringLiteral("view.workbench"), QStringLiteral("view.smidja")},
    };
    for (const QString &key : settings.allKeys()) {
        if (key.startsWith(QStringLiteral("keymap/"))) {
            const QString id = key.mid(7);
            const QString target = legacyWorkspaceCommands.value(id, id);
            // If both the old and the new id are present, the new one wins.
            if (target != id && overrides.contains(target))
                continue;
            overrides.insert(target, settings.value(key).toString());
        }
    }

    for (const Command &command : registry.all()) {
        QStringList sequences;
        if (overrides.contains(command.id)) {
            const QString override = overrides.value(command.id);
            if (!override.isEmpty())
                sequences = {override};
        } else {
            sequences = byCommand.value(command.id);
        }
        for (const QString &sequence : sequences)
            registry.bind(QKeySequence(sequence), command.id);
    }

    // Tab is consumed by focus navigation, so it needs a real shortcut too.
    auto *tabShortcut = new QShortcut(QKeySequence(Qt::Key_Tab), this);
    tabShortcut->setContext(Qt::WindowShortcut);
    m_globalShortcuts.append(tabShortcut);
    connect(tabShortcut, &QShortcut::activated, this, [this]() {
        if (qobject_cast<QLineEdit *>(QApplication::focusWidget())
            || qobject_cast<QTextEdit *>(QApplication::focusWidget())
            || qobject_cast<QPlainTextEdit *>(QApplication::focusWidget()))
            return;
        CommandContext context;
        context.sourcePath = m_source->uri();
        context.destPath = m_dest->uri();
        CommandRegistry::instance().run(QStringLiteral("panel.next"), context);
    });

    // Global shortcuts: every binding works even when a panel does not have
    // focus, but never steal keys from a text field.
    // Keys that belong to text editing or to the lister itself are left alone:
    // a window-wide shortcut would swallow them before the focused line edit
    // ever saw them. They still work whenever a lister has focus.
    static const QStringList textOwned = {
        QStringLiteral("Tab"),        QStringLiteral("Return"),
        QStringLiteral("Enter"),      QStringLiteral("Backspace"),
        QStringLiteral("Delete"),     QStringLiteral("Space"),
        QStringLiteral("Ctrl+C"),     QStringLiteral("Ctrl+X"),
        QStringLiteral("Ctrl+V"),     QStringLiteral("Ctrl+H"),
        QStringLiteral("Ctrl+L"),     QStringLiteral("Alt+Left"),
        QStringLiteral("Alt+Right"),  QStringLiteral("Alt+Up"),
    };

    for (const Command &command : CommandRegistry::instance().all()) {
        for (const QString &sequence : CommandRegistry::instance().shortcutsFor(command.id)) {
            if (textOwned.contains(sequence))
                continue;
            auto *shortcut = new QShortcut(QKeySequence(sequence), this);
            shortcut->setContext(Qt::WindowShortcut);
            m_globalShortcuts.append(shortcut);
            connect(shortcut, &QShortcut::activated, this, [this, id = command.id]() {
                if (qobject_cast<QLineEdit *>(QApplication::focusWidget())
                    || qobject_cast<QTextEdit *>(QApplication::focusWidget())
                    || qobject_cast<QPlainTextEdit *>(QApplication::focusWidget()))
                    return;
                CommandContext context;
                context.sourcePath = m_source->uri();
                context.destPath = m_dest->uri();
                context.selection = activePanel()->selection();
                CommandRegistry::instance().run(id, context);
            });
        }
    }
}

void MainWindow::buildButtonBank()
{
    m_toolbar = addToolBar(tr("Buttons"));
    m_toolbar->setObjectName(QStringLiteral("buttonBank"));
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toolbar->setIconSize(QSize(18, 18));

    CommandRegistry &registry = CommandRegistry::instance();

    // The bank is configuration, not code: first run writes the default so it
    // can be edited, and "-" inserts a separator.
    QStringList defaultBank;
    for (const char *id : kButtonBank)
        defaultBank.append(QString::fromLatin1(id));

    QSettings settings;
    QStringList bank = settings.value(QStringLiteral("ui/buttons")).toStringList();
    if (bank.isEmpty()) {
        bank = defaultBank;
        settings.setValue(QStringLiteral("ui/buttons"), bank);
    }

    for (const QString &commandId : bank) {
        if (commandId == QLatin1String("-")) {
            m_toolbar->addSeparator();
            continue;
        }

        const Command *command = registry.find(commandId);
        if (!command)
            continue;

        auto *button = new QToolButton(m_toolbar);
        button->setObjectName(commandId);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setIconSize(QSize(16, 16));

        const QString shortcut = registry.shortcutFor(commandId);
        button->setToolTip(shortcut.isEmpty()
                               ? command->title
                               : QStringLiteral("%1 (%2)").arg(command->title, shortcut));

        // Directory Opus keeps its actions readable: icon plus a word, never an
        // unlabelled glyph. Without an icon theme the word still stands alone.
        button->setText(command->title);
        button->setAccessibleName(command->title);
        const QIcon icon = iconForCommand(commandId);
        if (!icon.isNull()) {
            button->setIcon(icon);
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        } else {
            button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }
        connect(button, &QToolButton::clicked, this, [this, commandId]() {
            CommandContext context;
            context.sourcePath = m_source->uri();
            context.destPath = m_dest->uri();
            context.selection = activePanel()->selection();
            CommandRegistry::instance().run(commandId, context);
        });
        m_toolbar->addWidget(button);
    }
}
