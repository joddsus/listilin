#include "atomicfile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QUuid>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace AtomicFile {

QString stagingPath(const QString &target, const QString &tag)
{
    const QString dir = QFileInfo(target).absolutePath();
    return QDir(dir).filePath(QStringLiteral(".listista-%1-%2")
                                  .arg(tag, QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

bool publish(const QString &staged, const QString &target, QString *error)
{
    if (!QFileInfo::exists(staged) && !QFileInfo(staged).isSymLink()) {
        if (error)
            *error = QObject::tr("The staged copy of %1 is missing").arg(target);
        return false;
    }

    if (QFileInfo(target).isDir() && !QFileInfo(target).isSymLink()) {
        if (error)
            *error = QObject::tr("%1 is a folder and cannot be replaced by a file").arg(target);
        return false;
    }

    // Flush the staged file to disk before it becomes visible under the real
    // name, so a crash right after the rename cannot leave an empty file.
#ifdef Q_OS_UNIX
    {
        QFile stagedFile(staged);
        if (stagedFile.open(QIODevice::ReadOnly)) {
            const int descriptor = stagedFile.handle();
            if (descriptor >= 0)
                ::fsync(descriptor);
            stagedFile.close();
        }
    }
    if (::rename(QFile::encodeName(staged).constData(), QFile::encodeName(target).constData()) != 0) {
        if (error)
            *error = QObject::tr("Could not replace %1").arg(target);
        return false;
    }
    // Make the directory entry itself durable where the platform allows it.
    const QString dir = QFileInfo(target).absolutePath();
    QFile dirHandle(dir);
    if (dirHandle.open(QIODevice::ReadOnly)) {
        const int descriptor = dirHandle.handle();
        if (descriptor >= 0)
            ::fsync(descriptor);
        dirHandle.close();
    }
    return true;
#elif defined(Q_OS_WIN)
    const QString nativeStaged = QDir::toNativeSeparators(staged);
    const QString nativeTarget = QDir::toNativeSeparators(target);
    if (!::MoveFileExW(reinterpret_cast<const wchar_t *>(nativeStaged.utf16()),
                       reinterpret_cast<const wchar_t *>(nativeTarget.utf16()),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        if (error)
            *error = QObject::tr("Could not replace %1").arg(target);
        return false;
    }
    return true;
#else
    // No atomic replacement primitive: keep the old target until the new file
    // has been moved into place under a backup name.
    const QString backup = stagingPath(target, QStringLiteral("backup"));
    const bool hadTarget = QFileInfo::exists(target) || QFileInfo(target).isSymLink();
    if (hadTarget && !QFile::rename(target, backup)) {
        if (error)
            *error = QObject::tr("Could not replace %1").arg(target);
        return false;
    }
    if (!QFile::rename(staged, target)) {
        if (hadTarget)
            QFile::rename(backup, target);
        if (error)
            *error = QObject::tr("Could not replace %1").arg(target);
        return false;
    }
    if (hadTarget)
        QFile::remove(backup);
    return true;
#endif
}

bool write(const QString &target, const QByteArray &data, QString *error)
{
    const QString staged = stagingPath(target, QStringLiteral("save"));
    QFile out(staged);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QObject::tr("Could not write %1").arg(target);
        return false;
    }

    if (out.write(data) != data.size()) {
        out.close();
        QFile::remove(staged);
        if (error)
            *error = QObject::tr("Could not write %1 — is the disk full?").arg(target);
        return false;
    }

    const bool flushed = out.flush();
    out.close();
    if (!flushed || out.error() != QFileDevice::NoError) {
        QFile::remove(staged);
        if (error)
            *error = QObject::tr("Could not finish writing %1").arg(target);
        return false;
    }

    // Keep the mode of the file being replaced; a fresh file keeps the umask.
    const QFileInfo existing(target);
    if (existing.exists() && !existing.isSymLink())
        QFile::setPermissions(staged, existing.permissions());

    if (!publish(staged, target, error)) {
        QFile::remove(staged);
        return false;
    }
    return true;
}

} // namespace AtomicFile
