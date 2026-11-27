#include "fileshred.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
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

// A file with more than one hard link shares its contents with another name.
// Overwriting it would destroy the data behind the other name, so the link is
// simply removed instead; the contents stay alive under the remaining links.
bool hasOtherHardLinks(const QString &path)
{
#ifdef Q_OS_UNIX
    struct ::stat info;
    if (::lstat(QFile::encodeName(path).constData(), &info) != 0)
        return false;
    return info.st_nlink > 1;
#else
    Q_UNUSED(path)
    return false;
#endif
}

bool removeDirectory(const QString &path, QString *error)
{
    if (!QDir().rmdir(path)) {
        if (error)
            *error = QObject::tr("Could not remove %1").arg(path);
        return false;
    }
    return true;
}

} // namespace

namespace FileShred {

bool shredFile(const QString &path, int passes, QString *error)
{
    if (passes < 1)
        passes = 1;

    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) {
        if (error)
            *error = QObject::tr("%1 does not exist").arg(path);
        return false;
    }

    // Removing a symlink removes the link, never the file it points at.
    if (info.isSymLink()) {
        if (!QFile::remove(path)) {
            if (error)
                *error = QObject::tr("Could not remove the link %1").arg(path);
            return false;
        }
        return true;
    }

    if (info.isDir()) {
        if (error)
            *error = QObject::tr("%1 is a directory").arg(path);
        return false;
    }

    // Shredding one name of a hard-linked file would corrupt every other name.
    if (!hasOtherHardLinks(path)) {
        if (!overwriteFile(path, passes, error))
            return false;
    }

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
    if (!info.exists() && !info.isSymLink())
        return true;

    // A folder symlink is a link: shred the link, not the folder it names.
    if (info.isSymLink() || !info.isDir())
        return shredFile(path, passes, error);

    const QFileInfoList children = QDir(path).entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
    for (const QFileInfo &child : children) {
        QString childError;
        if (!shredTree(child.absoluteFilePath(), passes, &childError)) {
            if (error)
                *error = childError;
            return false;
        }
    }

    return removeDirectory(path, error);
}

} // namespace FileShred
