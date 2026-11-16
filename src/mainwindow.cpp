#include "mainwindow.h"
#include "command.h"
#include "commandpalette.h"
#include "dirmodel.h"
#include "fileops.h"
#include "filetask.h"
#include "operationdialog.h"
#include "shortcutsheet.h"
#include "listerview.h"
#include "panel.h"
#include "vfs.h"

#include "format.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QDirIterator>
#include <QKeySequenceEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QStandardPaths>
#include <QTextEdit>
#include <QMimeDatabase>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QFrame>
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
#include <QUrl>

namespace {

// The button bank is data: ids only. Everything a button can do is a command,
// which is why the same list can be edited in a config file.
const char *const kButtonBank[] = {
    "lister.historyBack", "lister.historyForward", "lister.parent", "lister.home",
    "lister.refresh",     "-",                     "file.mkdir",    "file.copy",
    "file.move",          "file.trash",            "-",             "lister.toggleHidden",
};

// Right-click menus, also data. There are two of them because they answer two
// different questions: "what can I do with this file" and "what can I do with
// this folder". Both are one click apart, and the switch is the first row, so
// landing on a file never hides the folder commands.
const QStringList &fileContextMenu()
{
    static const QStringList ids = {
        QStringLiteral("lister.open"),   QStringLiteral("file.view"),
        QStringLiteral("file.edit"),     QStringLiteral("file.rename"),
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
        QStringLiteral("file.mkdir"),          QStringLiteral("lister.refresh"),
        QStringLiteral("-"),                   QStringLiteral("lister.toggleHidden"),
        QStringLiteral("lister.focusPath"),    QStringLiteral("view.togglePlaces"),
        QStringLiteral("-"),                   QStringLiteral("panel.next"),
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

} // namespace

MainWindow::MainWindow(const QString &startPath, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Lister"));

    auto *panels = new QSplitter(Qt::Horizontal);
    panels->setChildrenCollapsible(false);
    panels->setHandleWidth(1);

    m_source = new Panel(panels);
    m_dest = new Panel(panels);
    panels->addWidget(m_source);
    panels->addWidget(m_dest);

    // A column of places on the left, because "where can I go" is the first
    // question a new user has.
    auto *placesBox = new QWidget(this);
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
    outer->addWidget(panels);
    outer->setStretchFactor(0, 0);
    outer->setStretchFactor(1, 1);
    outer->setSizes({180, 1000});
    setCentralWidget(outer);

    wirePanel(m_source);
    wirePanel(m_dest);

    resize(1180, 640);

    m_active = m_source;
    setActivePanel(m_source);
    restoreSession(startPath);

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
    });

    connect(panel, &Panel::contextMenuRequested, this, [this, panel](const QPoint &globalPos) {
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

    m_active = panel;
    m_source->setActive(panel == m_source);
    m_dest->setActive(panel == m_dest);
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

    bool accepted = false;
    const QString needle = QInputDialog::getText(this, tr("Find files"), tr("Name contains:"),
                                                 QLineEdit::Normal, QString(), &accepted);
    if (!accepted || needle.trimmed().isEmpty())
        return;

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
        if (it.fileName().contains(needle, Qt::CaseInsensitive))
            matches.append(it.filePath());
        if (matches.size() >= 5000)
            break;
        QApplication::processEvents();
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Find — %1").arg(needle));
    dialog.resize(680, 440);
    auto *layout = new QVBoxLayout(&dialog);
    auto *list = new QListWidget(&dialog);
    for (const QString &match : matches)
        list->addItem(QDir(root).relativeFilePath(match));
    layout->addWidget(new QLabel(tr("%1 matches").arg(matches.size()), &dialog));
    layout->addWidget(list, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
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
                      QSettings().setValue(QStringLiteral("ui/iconView"), icons);
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

    registry.add({QStringLiteral("view.togglePlaces"), tr("Show or hide places"), tr("View"),
                  [this](CommandContext &) { togglePlaces(); }});

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
        startTask(title, [items, targetDir, moveFiles](FileOps::Progress &progress, QString *error) {
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

void MainWindow::togglePlaces()
{
    if (!m_places)
        return;
    m_places->setVisible(!m_places->isVisible());
}

void MainWindow::showWelcome()
{
    QSettings().setValue(QStringLiteral("ui/welcomeShown"), true);

    QMessageBox box(this);
    box.setWindowTitle(tr("Welcome to Lister"));
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
           "what you want to do. <b>Ctrl+Shift+K</b> lists every shortcut.</p>"));
    box.setStandardButtons(QMessageBox::Ok);
    box.button(QMessageBox::Ok)->setText(tr("Get started"));
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

    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
}

void MainWindow::saveSession()
{
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
        editor->setMaximumSequenceLength(1);
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
    for (const QString &key : settings.allKeys()) {
        if (key.startsWith(QStringLiteral("keymap/")))
            overrides.insert(key.mid(7), settings.value(key).toString());
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
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

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

        const QIcon icon = iconForCommand(commandId);
        if (!icon.isNull()) {
            button->setIcon(icon);
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
            button->setAccessibleName(command->title);
        } else {
            // No icon theme available: fall back to words, never to a blank button.
            button->setText(command->title);
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
