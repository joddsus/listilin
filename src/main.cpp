#include "mainwindow.h"
#include "panel.h"
#include "settingsmigration.h"
#include "vfs.h"
#include "vfs_archive.h"
#include "vfs_curl.h"
#include "vfs_local.h"
#include "vfs_smb.h"

#include <memory>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>

#ifndef LISTISTA_VERSION
#define LISTISTA_VERSION "0.1.1"
#endif

namespace {

// The previous build stored its settings as Lister. Copy them once to the new
// Listista location; the old file is left untouched so a rollback still works.
void migrateLegacySettings()
{
    QSettings legacy(QStringLiteral("Lister"), QStringLiteral("Lister"));
    QSettings current(QStringLiteral("Listista"), QStringLiteral("Listista"));

    const QFileInfo currentFile(current.fileName());
    SettingsMigration::State state;
    state.legacyHasKeys = !legacy.allKeys().isEmpty();
    state.currentFileExists = currentFile.exists();
    // A file that exists but cannot be opened for reading is treated as
    // unreadable and is preserved instead of being overwritten.
    state.currentFileReadable = !state.currentFileExists
        || QFile(currentFile.absoluteFilePath()).open(QIODevice::ReadOnly);
    state.currentHasKeys = !current.allKeys().isEmpty();

    if (!SettingsMigration::shouldCopyLegacy(state))
        return;

    for (const QString &key : legacy.allKeys())
        current.setValue(key, legacy.value(key));
    current.sync();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setOrganizationName(QStringLiteral("Listista"));
    QApplication::setApplicationName(QStringLiteral("Listista"));
    QApplication::setApplicationDisplayName(QStringLiteral("Listista"));
    QApplication::setApplicationVersion(QString::fromLatin1(LISTISTA_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("listista"));

    migrateLegacySettings();

    // Every browsable namespace is registered here, in one place.
    Vfs::instance().addBackend(std::make_shared<LocalBackend>());
    Vfs::instance().addBackend(std::make_shared<ArchiveBackend>());
    Vfs::instance().addBackend(std::make_shared<CurlBackend>(QStringLiteral("ftp")));
    Vfs::instance().addBackend(std::make_shared<CurlBackend>(QStringLiteral("ftps")));
    Vfs::instance().addBackend(std::make_shared<CurlBackend>(QStringLiteral("ftpes")));
    Vfs::instance().addBackend(std::make_shared<CurlBackend>(QStringLiteral("sftp")));
    Vfs::instance().addBackend(std::make_shared<SmbBackend>());

    QString startPath;
    QString screenshotPath;

    const QStringList args = QApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if ((args.at(i) == QLatin1String("--path") || args.at(i) == QLatin1String("-p"))
            && i + 1 < args.size()) {
            startPath = args.at(++i);
        } else if (args.at(i) == QLatin1String("--screenshot") && i + 1 < args.size()) {
            screenshotPath = args.at(++i);
        }
    }

    MainWindow window(startPath);
    window.show();
    window.sourcePanel()->focusLister();

    // Headless verification hook: render one frame to PNG and exit, so the UI
    // can be inspected without a compositor.
    if (!screenshotPath.isEmpty()) {
        bool ok = false;
        const int delay = qEnvironmentVariableIntValue("LISTER_SCREENSHOT_DELAY", &ok);
        QTimer::singleShot(ok ? delay : 200, &window, [&window, screenshotPath]() {
            const QPixmap pixmap = window.grab();
            if (!pixmap.save(screenshotPath))
                qWarning("could not save screenshot to %s", qPrintable(screenshotPath));
            QApplication::quit();
        });
    }

    return app.exec();
}
