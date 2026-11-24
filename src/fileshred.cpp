#include "fileshred.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace {

bool overwriteFile(const QString &path, int passes, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadWrite)) {
        if (error)
            *error = QObject::tr("Could not open %1").arg(path);
        return false;
    }

    const qint64 size = file.size();
    for (int pass = 0; pass < passes; ++pass) {
        file.seek(0);
        QByteArray buffer(64 * 1024, Qt::Uninitialized);
        qint64 remaining = size;
        while (remaining > 0) {
            const qint64 chunk = qMin<qint64>(remaining, buffer.size());
            if (pass == passes - 1) {
                for (qint64 i = 0; i < chunk; ++i)
                    buffer[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xff);
            } else {
                buffer.fill('\0', static_cast<int>(chunk));
            }
            if (file.write(buffer.constData(), chunk) != chunk) {
                if (error)
                    *error = QObject::tr("Could not overwrite %1").arg(path);
                file.close();
                return false;
            }
            remaining -= chunk;
        }
        file.flush();
#ifdef Q_OS_UNIX
        if (file.handle() >= 0)
            ::fsync(file.handle());
#endif
    }
    file.close();
    return true;
}

} // namespace

namespace FileShred {

bool shredFile(const QString &path, int passes, QString *error)
{
    if (passes < 1)
        passes = 1;

    const QFileInfo info(path);
    if (!info.exists()) {
        if (error)
            *error = QObject::tr("%1 does not exist").arg(path);
        return false;
    }
    if (info.isDir()) {
        if (error)
            *error = QObject::tr("%1 is a directory").arg(path);
        return false;
    }

    if (!overwriteFile(path, passes, error))
        return false;

    if (!QFile::remove(path)) {
        if (error)
            *error = QObject::tr("Could not remove %1").arg(path);
        return false;
    }
    return true;
}

bool shredTree(const QString &path, int passes, QString *error)
{
    if (passes < 1)
        passes = 1;

    const QFileInfo info(path);
    if (!info.exists())
        return true;
    if (!info.isDir()) {
        return shredFile(path, passes, error);
    }

    // Shred files first, then remove directories.
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString fileError;
        if (!shredFile(it.filePath(), passes, &fileError)) {
            if (error)
                *error = fileError;
            return false;
        }
    }

    if (!QDir(path).removeRecursively()) {
        if (error)
            *error = QObject::tr("Could not remove %1").arg(path);
        return false;
    }
    return true;
}

} // namespace FileShred