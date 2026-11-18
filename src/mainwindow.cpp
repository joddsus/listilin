#include "mainwindow.h"
#include "command.h"
#include "commandpalette.h"
#include "dirmodel.h"
#include "fileops.h"
#include "filetask.h"
#include "foldercompare.h"
#include "operationdialog.h"
#include "shortcutsheet.h"
#include "listerview.h"
#include "panel.h"
#include "vfs.h"
#include "vfs_archive.h"

#include "format.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QMimeData>
#include <QPlainTextEdit>
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
        QStringLiteral("selection.sameType"),
        QStringLiteral("-"),             QStringLiteral("file.copy"),
                QStringLiteral("file.move"),     QStringLiteral("file.clipboardCopy"),
        QStringLiteral("file.clipboardPaste"), QStringLiteral("file.copyPath"),
        QStringLiteral("file.compress"),
        QStringLiteral("file.extract"),   QStringLiteral("file.trash"),     QStringLiteral("file.properties"),
    };
    return ids;
}

const QStringList &folderContextMenu()
{
    static const QStringList ids = {
        QStringLiteral("file.mkdir"),          QStringLiteral("tab.openFolder"),
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

QString temporaryCopy(const QString &uri, QString *error)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/lister-open");
    if (!QDir().mkpath(dir)) {
        if (error)
            *error = QObject::tr("Could not create %1").arg(dir);
        return {};
    }

    const QString target = QDir(dir).filePath(vfsLeafName(uri));
    if (!Vfs::instance().fetch(uri, target, error))
        return {};

    return target;
}

QString archivePasswordPrompt(const QString &path)
{
    bool ok = false;
    return QInputDialog::getText(nullptr, QObject::tr("Password"),
                                 QObject::tr("Enter password for %1").arg(QFileInfo(path).fileName()),
                                 QLineEdit::Password, QString(), &ok);
}

} // namespace

MainWindow::MainWindow(const QString &startPath, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Listista"));

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

    resize(1180, 640);

    newTab();

    m_active = m_source;
    setActivePanel(m_source);
    restoreSession(startPath);

    setArchivePasswordProvider(&archivePasswordPrompt);

    registerCommands();
    loadBindings();
    buildButtonBank();
    buildMenus();
    buildPlaces();

    const bool icons = QSettings().value(QStringLiteral("ui/iconView"), false).toBool();
    m_source->setIconView(icons);
    m_dest->setIconView(icons);

    if (!QSettings().value(QStringLiteral("ui/welcomeShown"), false).toBool())
        QTimer::singleShot(400, this, &MainWindow::showWelcome);

    // Sessions must survive every exit path, not only the window close button.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveSession);

    statusBar()->showMessage(
        tr("Ctrl+K command palette  ·  Alt+C copy  ·  Alt+M move  ·  Tab switch panel"));
}

void MainWindow::wirePanel(Panel *panel)
{
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

    connect(panel, &Panel::uriChanged, this, [this, panel](const QString &) {
        if (panel == m_active)
            setActivePanel(panel);

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

    for (const Tab &tab : m_tabs) {
        if (tab.source == panel || tab.dest == panel) {
            m_source = tab.source;
            m_dest = tab.dest;
            break;
        }
    }

    m_active = panel;
    m_source->setActive(panel == m_source);
    m_dest->setActive(panel == m_dest);
    updatePanelVisibility();
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
        m_active = m_source;
        setActivePanel(m_source);
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
    const QString local = temporaryCopy(uri, &error);
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
    QApplication::clipboard()->setMimeData(mime);
    m_clipboardCut = cut;
    statusBar()->showMessage(cut ? tr("Cut to clipboard.") : tr("Copied to clipboard."), 4000);
}

void MainWindow::clipboardPaste()
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!mime || !mime->hasUrls()) {
        statusBar()->showMessage(tr("The clipboard has no files."), 4000);
        return;
    }

    QVector<FileOps::CopyItem> items;
    for (const QUrl &url : mime->urls()) {
        FileOps::CopyItem item;
        if (url.isLocalFile()) {
            item.uri = QUrl::fromLocalFile(url.toLocalFile()).toString();
            item.isDir = QFileInfo(url.toLocalFile()).isDir();
        } else {
            item.uri = url.toString();
            item.isDir = false;
        }
        items.append(item);
    }

    runTransfer(items, activePanel()->uri(), m_clipboardCut);
    m_clipboardCut = false;
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
                target = temporaryCopy(uri, &error);
                if (target.isEmpty()) {
                    QMessageBox::warning(this, tr("Open"), error);
                    continue;
                }
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(target));
            continue;
        }

        // Read the content.
        QString content;
        QString tempPath;
        bool needUpload = false;
        if (Vfs::isLocal(uri)) {
            QFile f(Vfs::localPathOf(uri));
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                content = QString::fromUtf8(f.readAll());
                f.close();
            }
        } else {
            QString error;
            tempPath = temporaryCopy(uri, &error);
            if (tempPath.isEmpty()) {
                QMessageBox::warning(this, tr("Open"), error);
                continue;
            }
            QFile f(tempPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                content = QString::fromUtf8(f.readAll());
                f.close();
            }
            needUpload = Vfs::instance().writable(VfsUri::parentUri(uri));
        }

        QDialog dialog(this);
        dialog.setWindowTitle(edit ? tr("Edit — %1").arg(name) : tr("View — %1").arg(name));
        dialog.resize(720, 480);
        auto *layout = new QVBoxLayout(&dialog);
        auto *editor = new QPlainTextEdit(&dialog);
        editor->setPlainText(content);
        editor->setReadOnly(!edit);
        layout->addWidget(editor);
        auto *buttons = new QDialogButtonBox(&dialog);
        if (edit) {
            buttons->addButton(QDialogButtonBox::Save);
            buttons->addButton(QDialogButtonBox::Cancel);
        } else {
            buttons->addButton(QDialogButtonBox::Close);
        }
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted || !edit)
            continue;

        const QByteArray newData = editor->toPlainText().toUtf8();
        if (Vfs::isLocal(uri)) {
            QFile f(Vfs::localPathOf(uri));
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                f.write(newData);
                f.close();
            }
        } else if (needUpload) {
            const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                + QStringLiteral("/lister-edit");
            QDir().mkpath(tempDir);
            const QString savePath = QDir(tempDir).filePath(name);
            QFile f(savePath);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                f.write(newData);
                f.close();
                QString parentUri = VfsUri::parentUri(uri);
                QString error;
                Vfs::instance().put(parentUri, name, savePath, &error);
                QFile::remove(savePath);
            }
        }
    }
}

void MainWindow::findFiles()
{
    Panel *panel = activePanel();
    if (!Vfs::isLocal(panel->uri())) {
        QMessageBox::information(this, tr("Find files"),
                                 tr("Search works in local folders for now."));
        return;
    }

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

    const QString root = Vfs::localPathOf(panel->uri());
    QProgressDialog progress(tr("Searching…"), tr("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(300);

    QStringList matches;
    QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (progress.wasCanceled())
            break;
        it.next();
        const QFileInfo info = it.fileInfo();
        if (!dirsCheck->isChecked() && info.isDir())
            continue;
        if (!needle.isEmpty() && !info.fileName().contains(needle, Qt::CaseInsensitive))
            continue;
        if (!ext.isEmpty()) {
            const QString suffix = info.suffix().toLower();
            if (suffix != ext.toLower().remove(QLatin1Char('.')))
                continue;
        }
        if (minBytes > 0 && info.size() < minBytes)
            continue;
        if (maxBytes >= 0 && info.size() > maxBytes)
            continue;
        if (cutoff.isValid() && info.lastModified() < cutoff)
            continue;
        matches.append(it.filePath());
        if (matches.size() >= 5000)
            break;
        QApplication::processEvents();
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Find results"));
    dialog.resize(680, 440);
    auto *layout = new QVBoxLayout(&dialog);
    auto *list = new QListWidget(&dialog);
    for (const QString &match : matches)
        list->addItem(QDir(root).relativeFilePath(match));
    layout->addWidget(new QLabel(tr("%1 matches").arg(matches.size()), &dialog));
    layout->addWidget(list, 1);
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(btns);
    connect(btns, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(list, &QListWidget::itemActivated, &dialog, [&dialog](QListWidgetItem *item) {
        if (item)
            dialog.setProperty("selected", item->text());
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString relative = dialog.property("selected").toString();
    if (relative.isEmpty())
        return;

    const QString path = QDir(root).filePath(relative);
    panel->setUri(QUrl::fromLocalFile(QFileInfo(path).absolutePath()).toString());
    panel->selectName(QFileInfo(path).fileName());
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

    if (result.items.isEmpty()) {
        out += tr("The folders match.\n");
    } else {
        appendSection(tr("Only in left"), onlyLeft);
        appendSection(tr("Only in right"), onlyRight);
        appendSection(tr("Type differs"), typeChanged);
        appendSection(tr("Size differs"), sizeChanged);
        appendSection(tr("Content differs"), contentChanged);
        appendSection(tr("Link target differs"), linkChanged);
        appendSection(tr("Could not read"), readErrors);
    }

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

                      QString error;
                      if (Vfs::isLocal(panel->uri())) {
                          QString created;
                          if (!FileOps::makeDirectory(Vfs::localPathOf(panel->uri()), name.trimmed(),
                                                      &created, &error)) {
                              QMessageBox::warning(this, tr("New folder"), error);
                              return;
                          }
                          panel->model()->refresh();
                          panel->selectName(QFileInfo(created).fileName());
                          return;
                      }

                      if (!Vfs::instance().mkdir(panel->uri(), name.trimmed(), &error)) {
                          QMessageBox::warning(this, tr("New folder"), error);
                          return;
                      }
                      panel->model()->refresh();
                      panel->selectName(name.trimmed());
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

                      QString error;
                      if (Vfs::isLocal(panel->uri())) {
                          QString renamed;
                          if (!FileOps::rename(Vfs::localPathOf(panel->currentUri()), name.trimmed(),
                                               &renamed, &error)) {
                              QMessageBox::warning(this, tr("Rename"), error);
                              return;
                          }
                          panel->model()->refresh();
                          panel->selectName(QFileInfo(renamed).fileName());
                          return;
                      }

                      QString newUri;
                      if (!Vfs::instance().rename(panel->currentUri(), name.trimmed(), &newUri, &error)) {
                          QMessageBox::warning(this, tr("Rename"), error);
                          return;
                      }
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

                      auto updatePreview = [&]() {
                          QSet<QString> seen;
                          for (int i = 0; i < names.size(); ++i) {
                              QFileInfo fi(names.at(i));
                              QString base = fi.baseName();
                              const QString ext = fi.suffix();
                              if (!findEdit->text().isEmpty())
                                  base.replace(findEdit->text(), replaceEdit->text());
                              if (!prefixEdit->text().isEmpty())
                                  base = prefixEdit->text() + base;
                              if (!suffixEdit->text().isEmpty())
                                  base += suffixEdit->text();
                              if (numbering->isChecked())
                                  base += QString::number(startSpin->value() + i);
                              QString newName = base;
                              if (keepExt->isChecked() && !ext.isEmpty())
                                  newName += QLatin1Char('.') + ext;
                              if (newName.isEmpty())
                                  newName = names.at(i);
                              // flag duplicates
                              QString display = newName;
                              if (seen.contains(newName))
                                  display += tr("  (duplicate)");
                              seen.insert(newName);
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

                      // Apply.
                      int failed = 0;
                      QStringList errors;
                      for (int i = 0; i < uris.size(); ++i) {
                          const QString newName = preview->item(i, 1)->text().split(tr("  (duplicate)")).first().trimmed();
                          if (newName.isEmpty() || newName == names.at(i))
                              continue;
                          QString newUri;
                          QString error;
                          if (!Vfs::instance().rename(uris.at(i), newName, &newUri, &error)) {
                              failed++;
                              errors.append(error);
                          }
                      }
                      panel->model()->refresh();
                      if (failed > 0)
                          QMessageBox::warning(this, tr("Rename"),
                                              tr("%1 of %2 renames failed.\n%3")
                                                  .arg(failed).arg(uris.size())
                                                  .arg(errors.join(QLatin1Char('\n'))));
                      else
                          statusBar()->showMessage(tr("Renamed %1 items.").arg(uris.size()), 4000);
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

                      QString error;
                      if (!FileOps::moveToTrash(paths, &error)) {
                          // No usable trash in this session (some containers and
                          // network homes have none), so offer the honest choice
                          // instead of silently doing nothing.
                          const auto answer = QMessageBox::question(
                              this, tr("Trash"),
                              tr("The trash is not available here.\n\nDelete %1 "
                                 "permanently?")
                                  .arg(itemCount(int(paths.size()))),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                          if (answer != QMessageBox::Yes)
                              return;

                          startTask(tr("Deleting %1").arg(itemCount(int(paths.size()))),
                                    [paths](FileOps::Progress &progress, QString *deleteError) {
                                        FileOps::removePermanently(paths, deleteError, progress);
                                        return QStringList();
                                    });
                          return;
                      }

                      panel->model()->refresh();
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
                              const QString local = temporaryCopy(uri, &error);
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

    registry.add({QStringLiteral("bookmark.add"), tr("Add bookmark"), tr("Navigation"),
                  [this](CommandContext &) {
                      QSettings settings;
                      QStringList bookmarks = settings.value(QStringLiteral("ui/bookmarks")).toStringList();
                      const QString uri = activePanel()->uri();
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
                      auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                      layout->addRow(buttons);
                      connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                      connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                      if (dialog.exec() != QDialog::Accepted)
                          return;
                      QString uri = QStringLiteral("%1://").arg(scheme->currentText());
                      if (!user->text().isEmpty()) {
                          uri += user->text();
                          if (!pass->text().isEmpty())
                              uri += QStringLiteral(":") + pass->text();
                          uri += QStringLiteral("@");
                      }
                      uri += host->text();
                      if (!port->text().isEmpty())
                          uri += QStringLiteral(":") + port->text();
                      uri += path->text().isEmpty() ? QStringLiteral("/") : path->text();
                      activePanel()->setUri(uri);
                      activePanel()->focusLister();
                  }});

    registry.add({QStringLiteral("tools.compare"), tr("Compare folders"), tr("Tools"),
                  [this](CommandContext &) { compareFolders(); }});

    registry.add({QStringLiteral("tools.terminal"), tr("Open terminal here"), tr("Tools"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      QString dir = Vfs::isLocal(panel->uri()) ? Vfs::localPathOf(panel->uri())
                                                              : QDir::homePath();
                      QProcess::startDetached(QStringLiteral("x-terminal-emulator"),
                                             {QStringLiteral("--working-directory=") + dir});
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

                      if (!anyLocal || uris.size() != 1) {
                          for (const QString &uri : uris) {
                              QString local = Vfs::isLocal(uri) ? Vfs::localPathOf(uri) : QString();
                              if (local.isEmpty()) {
                                  QString error;
                                  local = temporaryCopy(uri, &error);
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

                      const QString path = Vfs::localPathOf(uris.first());
                      const QFileInfo info(path);
                      QDialog dialog(this);
                      dialog.setWindowTitle(tr("Properties — %1").arg(info.fileName()));
                      auto *layout = new QVBoxLayout(&dialog);
                      layout->addWidget(new QLabel(tr("Location: %1").arg(info.absolutePath()), &dialog));
                      layout->addWidget(new QLabel(tr("Size: %1").arg(humanSize(info.size())), &dialog));
                      layout->addWidget(new QLabel(tr("Modified: %1").arg(info.lastModified().toString()), &dialog));

                      auto *group = new QGroupBox(tr("Permissions"), &dialog);
                      auto *grid = new QGridLayout(group);
                      QStringList labels = {tr("Read"), tr("Write"), tr("Execute")};
                      QFile::Permissions perms = info.permissions();
                      QCheckBox *boxes[3][3] = {};
                      const QFile::Permissions table[3][3] = {
                          {QFile::ReadOwner, QFile::WriteOwner, QFile::ExeOwner},
                          {QFile::ReadGroup, QFile::WriteGroup, QFile::ExeGroup},
                          {QFile::ReadOther, QFile::WriteOther, QFile::ExeOther},
                      };
                      QStringList rows = {tr("Owner"), tr("Group"), tr("Other")};
                      for (int r = 0; r < 3; ++r) {
                          grid->addWidget(new QLabel(rows[r], group), r + 1, 0);
                          for (int c = 0; c < 3; ++c) {
                              if (r == 0 && c == 0)
                                  grid->addWidget(new QLabel(labels[c], group), 0, c + 1);
                              boxes[r][c] = new QCheckBox(group);
                              boxes[r][c]->setChecked(perms & table[r][c]);
                              grid->addWidget(boxes[r][c], r + 1, c + 1);
                          }
                      }
                      layout->addWidget(group);

                      auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                      layout->addWidget(buttons);
                      connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                      connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

                      if (dialog.exec() != QDialog::Accepted)
                          return;

                      QFile::Permissions newPerms = QFile::Permissions(0);
                      for (int r = 0; r < 3; ++r)
                          for (int c = 0; c < 3; ++c)
                              if (boxes[r][c]->isChecked())
                                  newPerms |= table[r][c];
                      QFile::setPermissions(path, newPerms);
                      panel->model()->refresh();
                  }});

    registry.add({QStringLiteral("file.restore"), tr("Restore from trash"), tr("Files"),
                  [this](CommandContext &) {
                      Panel *panel = activePanel();
                      const QStringList uris = panel->selectedUris();
                      if (uris.isEmpty())
                          return;
                      QStringList names;
                      for (const QString &uri : uris)
                          names.append(vfsLeafName(uri));
                      QString error;
                      if (!FileOps::restoreFromTrash(names, &error)) {
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
                          if (info.isDir())
                              FileOps::copyRecursively(src, dest, nullptr, FileOps::Progress());
                          else
                              QFile::copy(src, dest);
                      }
                      panel->model()->refresh();
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

void MainWindow::runTransfer(const QVector<FileOps::CopyItem> &items, const QString &targetUri,
                             bool moveFiles)
{
    m_conflictMode = 0;

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
    const QString title = moveFiles ? tr("Moving %1").arg(itemCount(count))
                                    : tr("Copying %1").arg(itemCount(count));

    if (Vfs::isLocal(targetUri)) {
        const QString targetDir = Vfs::localPathOf(targetUri);
        startTask(title, [this, items, targetDir, moveFiles](FileOps::Progress &progress, QString *error) {
            progress.conflict = [this](const QString &source, const QString &target) {
                int result = FileOps::ConflictSkip;
                QMetaObject::invokeMethod(this, [this, &result, source, target]() {
                    result = resolveConflict(source, target);
                }, Qt::BlockingQueuedConnection);
                return result;
            };
            return FileOps::copyUris(items, targetDir, moveFiles, error, progress);
        });
        return;
    }

    // Writing into an archive or remote folder: upload each item through the VFS.
    startTask(title, [items, targetUri, moveFiles](FileOps::Progress &progress, QString *error) {
        QStringList failed;

        std::function<bool(const QString &, const QString &, bool)> uploadTree =
            [&](const QString &sourceUri, const QString &targetParentUri, bool isDir) -> bool {
                if (progress.cancelled && progress.cancelled())
                    return false;

                if (isDir) {
                    const QString name = vfsLeafName(sourceUri);
                    QString mkError;
                    if (!Vfs::instance().mkdir(targetParentUri, name, &mkError)) {
                        if (error)
                            *error = mkError;
                        return false;
                    }
                    const QString childTarget = VfsUri::childUri(targetParentUri, name);

                    QVector<Entry> entries;
                    QString listError;
                    if (!Vfs::instance().list(sourceUri, true, &entries, &listError)) {
                        // Source is not a folder we can list; treat as opaque.
                        return true;
                    }
                    for (const Entry &entry : entries) {
                        if (progress.cancelled && progress.cancelled())
                            return false;
                        const QString childSource = VfsUri::childUri(sourceUri, entry.name);
                        if (!uploadTree(childSource, childTarget, entry.isDir))
                            return false;
                    }
                    return true;
                }

                const QString name = vfsLeafName(sourceUri);
                if (progress.current)
                    progress.current(name);

                QString localPath;
                bool needCleanup = false;
                if (Vfs::isLocal(sourceUri)) {
                    localPath = Vfs::localPathOf(sourceUri);
                } else {
                    // Fetch the remote/archive entry to a temp file first.
                    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + QStringLiteral("/lister-upload");
                    QDir().mkpath(tempDir);
                    localPath = QDir(tempDir).filePath(name);
                    QString fetchError;
                    if (!Vfs::instance().fetch(sourceUri, localPath, &fetchError)) {
                        if (error)
                            *error = fetchError;
                        return false;
                    }
                    needCleanup = true;
                }

                QString putError;
                const bool ok = Vfs::instance().put(targetParentUri, name, localPath, &putError);
                if (needCleanup)
                    QFile::remove(localPath);
                if (!ok) {
                    if (error)
                        *error = putError;
                    return false;
                }

                if (progress.step)
                    progress.step(QFileInfo(localPath).size());
                if (progress.itemDone)
                    progress.itemDone();
                return true;
            };

        for (const FileOps::CopyItem &item : items) {
            if (progress.cancelled && progress.cancelled())
                break;

            const QString name = vfsLeafName(item.uri);
            const bool ok = uploadTree(item.uri, targetUri, item.isDir);
            if (!ok) {
                failed.append(item.uri);
                continue;
            }

            if (moveFiles) {
                if (Vfs::isLocal(item.uri)) {
                    const QFileInfo info(Vfs::localPathOf(item.uri));
                    if (info.isDir())
                        QDir(Vfs::localPathOf(item.uri)).removeRecursively();
                    else
                        QFile::remove(Vfs::localPathOf(item.uri));
                } else {
                    QString removeError;
                    Vfs::instance().remove(item.uri, true, &removeError);
                }
            }
        }

        return failed;
    });
}

int MainWindow::resolveConflict(const QString &source, const QString &target)
{
    if (m_conflictMode == 1)
        return FileOps::ConflictReplace;
    if (m_conflictMode == 2)
        return FileOps::ConflictSkip;

    QMessageBox box(this);
    box.setWindowTitle(tr("File already exists"));
    box.setText(tr("\"%1\" already exists at the destination.").arg(QFileInfo(target).fileName()));
    box.setInformativeText(tr("Source: %1\nDestination: %2").arg(source, target));
    QAbstractButton *replaceBtn = box.addButton(tr("Replace"), QMessageBox::AcceptRole);
    QAbstractButton *skipBtn = box.addButton(tr("Skip"), QMessageBox::RejectRole);
    QAbstractButton *replaceAllBtn = box.addButton(tr("Replace All"), QMessageBox::AcceptRole);
    QAbstractButton *skipAllBtn = box.addButton(tr("Skip All"), QMessageBox::RejectRole);
    QAbstractButton *abortBtn = box.addButton(tr("Abort"), QMessageBox::DestructiveRole);
    box.setDefaultButton(qobject_cast<QPushButton *>(skipBtn));
    box.exec();
    QAbstractButton *clicked = box.clickedButton();
    if (clicked == replaceBtn)
        return FileOps::ConflictReplace;
    if (clicked == skipBtn)
        return FileOps::ConflictSkip;
    if (clicked == replaceAllBtn) {
        m_conflictMode = 1;
        return FileOps::ConflictReplace;
    }
    if (clicked == skipAllBtn) {
        m_conflictMode = 2;
        return FileOps::ConflictSkip;
    }
    return FileOps::ConflictAbort;
}

bool MainWindow::startTask(const QString &title, const FileTask::Job &job)
{
    if (m_task) {
        m_pendingTasks.enqueue(PendingTask{title, job});
        statusBar()->showMessage(tr("“%1” queued — %2 waiting.")
                                     .arg(title)
                                     .arg(m_pendingTasks.size()),
                                 5000);
        return false;
    }

    m_task = FileTask::start(title, job, this);

    auto *dialog = new OperationDialog(title, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    m_operation = dialog;

    connect(m_task, &FileTask::progress, dialog,
            [this](qint64 bytes, qint64 total, const QString &current, int items) {
                if (m_operation)
                    m_operation->showProgress(bytes, total, current, items);
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
    startTask(next.title, next.job);
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

    statusBar()->showMessage(tr("Done — %1.").arg(itemCount(outcome.transferred)), 5000);
    startNextTask();
}

void MainWindow::buildMenus()
{
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
    const bool places = workspace != Workspace::Einn;

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
        source = settings.value(QStringLiteral("startup/source"), QDir::homePath()).toString();
    if (!QFileInfo(Vfs::localPathOf(source)).isDir() && !VfsUri::parse(source).isRemote()
        && !VfsUri::parse(source).isArchive()) {
        source = QDir::homePath();
    }

    QString dest = settings.value(QStringLiteral("startup/dest"), QDir::homePath()).toString();
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

    QSettings settings;
    settings.setValue(QStringLiteral("startup/source"), m_source->uri());
    settings.setValue(QStringLiteral("startup/dest"), m_dest->uri());
    settings.setValue(QStringLiteral("view/showHidden"), m_source->model()->showHidden());
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSession();
    QMainWindow::closeEvent(event);
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
