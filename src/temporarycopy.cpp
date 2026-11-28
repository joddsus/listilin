#include "temporarycopy.h"

#include "pathsafety.h"
#include "vfs.h"

#include <QDir>
#include <QObject>
#include <QStandardPaths>
#include <QUuid>

namespace TemporaryCopy {

QString ofUri(const QString &uri, QString *error)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/lister-open");
    // A fresh folder per opening: the file name can stay the same without two
    // archives (or two Listista processes) sharing one local path.
    const QString dir = base + QLatin1Char('/')
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().mkpath(dir)) {
        if (error)
            *error = QObject::tr("Could not create %1").arg(dir);
        return {};
    }

    QString name = vfsLeafName(uri);
    if (!PathSafety::isSafePathComponent(name))
        name = QStringLiteral("item");
    const QString target = QDir(dir).filePath(name);
    if (!Vfs::instance().fetch(uri, target, error))
        return {};

    return target;
}

} // namespace TemporaryCopy
