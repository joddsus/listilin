#include "mainwindow.h"
#include "panel.h"
#include "vfs.h"
#include "vfs_archive.h"
#include "vfs_curl.h"
#include "vfs_local.h"
#include "vfs_smb.h"

#include <memory>

#include <QApplication>
#include <QDir>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Lister"));
    QApplication::setApplicationDisplayName(QStringLiteral("Lister"));
    QApplication::setOrganizationName(QStringLiteral("Lister"));

    // Every browsable namespace is registered here, in one place.
    Vfs::instance().addBackend(std::make_shared<LocalBackend>());
    Vfs::instance().addBackend(std::make_shared<ArchiveBackend>());
    Vfs::instance().addBackend(std::make_shared<CurlBackend>(QStringLiteral("ftp")));
    Vfs::instance().addBackend(std::make_shared<CurlBackend>(QStringLiteral("ftps")));
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
