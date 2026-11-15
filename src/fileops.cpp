#include "fileops.h"
#include "vfs.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace {

// 256 KB per read keeps the progress smooth on a slow disk without drowning
// the UI thread in updates.
constexpr qint64 kCopyChunk = 256 * 1024;

bool cancelledNow(const FileOps::Progress &progress)
{
    return progress.cancelled && progress.cancelled();
}

// Bytes that a copy is about to move, so the progress bar can start out honest
// instead of creeping up on its own.
qint64 treeBytes(const QString &path)
{
    const QFileInfo info(path);
    if (info.isSymLink() || !info.isDir())
        return info.isDir() ? 0 : info.size();

    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

bool copyFileData(const QString &source, const QString &target, QString *error,
                  const FileOps::Progress &progress)
{
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QObject::tr("Could not read %1").arg(source);
        return false;
    }

    if (QFile::exists(target) && !QFile::remove(target)) {
        if (error)
            *error = QObject::tr("Could not overwrite %1").arg(target);
        return false;
    }

    QFile out(target);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QObject::tr("Could not write %1").arg(target);
        return false;
    }

    QByteArray buffer;
    buffer.resize(int(kCopyChunk));
    bool ok = true;

    while (true) {
        if (cancelledNow(progress)) {
            ok = false;
            break;
        }

        const qint64 read = in.read(buffer.data(), buffer.size());
        if (read < 0) {
            if (error)
                *error = QObject::tr("Could not read %1").arg(source);
            ok = false;
            break;
        }
        if (read == 0)
            break;

        if (out.write(buffer.constData(), read) != read) {
            if (error)
                *error = QObject::tr("Writing stopped at %1 — is the disk full?").arg(target);
            ok = false;
            break;
        }

        if (progress.step)
            progress.step(read);
    }

    const QFile::Permissions permissions = in.permissions();
    out.close();
    in.close();

    if (!ok) {
        // Never leave half a file behind looking like a finished copy.
        QFile::remove(target);
        return false;
    }

    // Keep the timestamp before the permissions: a read-only source file could
    // not be reopened afterwards.
    const QFileInfo info(source);
    if (info.lastModified().isValid()) {
        QFile device(target);
        if (device.open(QIODevice::ReadWrite))
            device.setFileTime(info.lastModified(), QFileDevice::FileModificationTime);
    }
    QFile::setPermissions(target, permissions);
    return true;
}

bool copyRecursively(const QString &source, const QString &target, QString *error,
                     const FileOps::Progress &progress)
{
    if (cancelledNow(progress))
        return false;

    const QFileInfo info(source);

    // A symlink is copied as a symlink: following it would duplicate data and
    // could loop forever on a directory that points at its own parent.
    if (info.isSymLink()) {
        if (QFileInfo::exists(target) && !QFile::remove(target)) {
            if (error)
                *error = QObject::tr("Could not overwrite %1").arg(target);
            return false;
        }
        if (!QFile::link(info.symLinkTarget(), target)) {
            if (error)
                *error = QObject::tr("Could not create the link %1").arg(target);
            return false;
        }
        if (progress.itemDone)
            progress.itemDone();
        return true;
    }

    if (info.isDir()) {
        QDir dir(target);
        if (!dir.exists() && !QDir().mkpath(target)) {
            if (error)
                *error = QObject::tr("Could not create %1").arg(target);
            return false;
        }

        if (progress.current)
            progress.current(info.fileName());

        const QFileInfoList children
            = QDir(source).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const QFileInfo &child : children) {
            if (!copyRecursively(child.absoluteFilePath(),
                                 QDir(target).filePath(child.fileName()), error, progress))
                return false;
        }
        return true;
    }

    if (progress.current)
        progress.current(info.fileName());

    if (!copyFileData(source, target, error, progress))
        return false;

    if (progress.itemDone)
        progress.itemDone();
    return true;
}

QString uniqueTarget(const QString &targetDir, const QString &name)
{
    QString candidate = QDir(targetDir).filePath(name);
    if (!QFileInfo::exists(candidate))
        return candidate;

    const QFileInfo info(name);
    const QString base = info.completeBaseName().isEmpty() ? name : info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix();

    for (int i = 2; i < 10000; ++i) {
        candidate = QDir(targetDir).filePath(QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(suffix));
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    return QDir(targetDir).filePath(name);
}

} // namespace

namespace FileOps {

QStringList copyAll(const QStringList &sources, const QString &targetDir, bool moveFiles,
                    QString *error, const Progress &progress)
{
    QStringList failed;

    if (!QFileInfo(targetDir).isDir()) {
        if (error)
            *error = QObject::tr("%1 is not a folder").arg(targetDir);
        return sources;
    }

    qint64 totalBytes = 0;
    for (const QString &source : sources)
        totalBytes += treeBytes(source);
    if (progress.total)
        progress.total(totalBytes);

    for (const QString &source : sources) {
        if (cancelled(progress))
            break;

        const QFileInfo info(source);
        const QString target = QDir(targetDir).filePath(info.fileName());

        if (QFileInfo(target) == info)
            continue; // same file, nothing to do

        if (moveFiles) {
            if (info.isDir()) {
                if (!copyRecursively(source, uniqueTarget(targetDir, info.fileName()), error,
                                     progress)
                    || !QDir(source).removeRecursively()) {
                    failed.append(source);
                }
            } else {
                const QString destination = uniqueTarget(targetDir, info.fileName());
                if (!QFile::rename(source, destination)) {
                    if (!copyRecursively(source, destination, error, progress)
                        || !QFile::remove(source)) {
                        failed.append(source);
                        if (error)
                            *error = QObject::tr("Could not move %1").arg(source);
                    }
                }
            }
        } else {
            const QString destination = uniqueTarget(targetDir, info.fileName());
            if (!copyRecursively(source, destination, error, progress))
                failed.append(source);
        }
    }

    return failed;
}

namespace {

// Walks a folder that lives inside an archive or on a remote site, creating
// the same shape locally.
bool copyRemoteTree(const QString &uri, const QString &target, QString *error,
                    const FileOps::Progress &progress)
{
    if (cancelledNow(progress))
        return false;

    QVector<Entry> entries;
    if (!Vfs::instance().list(uri, true, &entries, error))
        return false;

    if (!QDir().mkpath(target)) {
        if (error)
            *error = QObject::tr("Could not create %1").arg(target);
        return false;
    }

    for (const Entry &entry : entries) {
        if (cancelledNow(progress))
            return false;

        const QString childUri = VfsUri::childUri(uri, entry.name);
        const QString childTarget = QDir(target).filePath(entry.name);

        if (entry.isDir) {
            if (!copyRemoteTree(childUri, childTarget, error, progress))
                return false;
            continue;
        }

        if (progress.current)
            progress.current(entry.name);

        if (!Vfs::instance().fetch(childUri, childTarget, error))
            return false;

        // Remote sizes only become known as the listing arrives, so the total is
        // adjusted on the way instead of guessed up front.
        if (progress.total)
            progress.total(entry.size);
        if (progress.step)
            progress.step(entry.size);
        if (progress.itemDone)
            progress.itemDone();
    }

    return true;
}

} // namespace

QStringList copyUris(const QVector<CopyItem> &items, const QString &targetDir, bool moveFiles,
                     QString *error, const Progress &progress)
{
    QStringList failed;

    if (!QFileInfo(targetDir).isDir()) {
        if (error)
            *error = QObject::tr("%1 is not a folder").arg(targetDir);
        QStringList uris;
        for (const CopyItem &item : items)
            uris.append(item.uri);
        return uris;
    }

    qint64 totalBytes = 0;
    for (const CopyItem &item : items) {
        const VfsUri parsed = VfsUri::parse(item.uri);
        if (parsed.isLocal())
            totalBytes += treeBytes(parsed.path);
    }
    if (progress.total)
        progress.total(totalBytes);

    for (const CopyItem &item : items) {
        if (cancelled(progress))
            break;

        const VfsUri parsed = VfsUri::parse(item.uri);
        const QString name = vfsLeafName(item.uri);
        if (name.isEmpty())
            continue;

        const QString target = uniqueTarget(targetDir, name);

        if (parsed.isLocal()) {
            QString copyError;
            if (!copyRecursively(parsed.path, target, &copyError, progress)) {
                if (!cancelled(progress) && error)
                    *error = copyError;
                failed.append(item.uri);
                continue;
            }

            if (moveFiles) {
                const bool removed = QFileInfo(parsed.path).isDir()
                    ? QDir(parsed.path).removeRecursively()
                    : QFile::remove(parsed.path);
                if (!removed) {
                    if (error)
                        *error = QObject::tr("Could not remove %1").arg(name);
                    failed.append(item.uri);
                }
            }
            continue;
        }

        // Archive and remote entries are pulled out through their backend.
        if (progress.current)
            progress.current(name);

        QString fetchError;
        const bool ok = item.isDir ? copyRemoteTree(item.uri, target, &fetchError, progress)
                                   : Vfs::instance().fetch(item.uri, target, &fetchError);
        if (!ok) {
            if (!cancelled(progress) && error)
                *error = fetchError;
            failed.append(item.uri);
        }
    }

    return failed;
}

bool makeDirectory(const QString &parentDir, const QString &name, QString *newPath, QString *error)
{
    const QString path = QDir(parentDir).filePath(name);
    if (QFileInfo::exists(path)) {
        if (error)
            *error = QObject::tr("%1 already exists").arg(name);
        return false;
    }

    if (!QDir().mkpath(path)) {
        if (error)
            *error = QObject::tr("Could not create %1").arg(name);
        return false;
    }

    if (newPath)
        *newPath = path;
    return true;
}

bool rename(const QString &path, const QString &newName, QString *newPath, QString *error)
{
    const QFileInfo info(path);
    const QString target = QDir(info.absolutePath()).filePath(newName);

    if (target == info.absoluteFilePath()) {
        if (newPath)
            *newPath = target;
        return true;
    }

    if (QFileInfo::exists(target)) {
        if (error)
            *error = QObject::tr("%1 already exists").arg(newName);
        return false;
    }

    if (!QFile::rename(path, target)) {
        if (error)
            *error = QObject::tr("Could not rename %1").arg(info.fileName());
        return false;
    }

    if (newPath)
        *newPath = target;
    return true;
}

namespace {

QString trashBaseDir()
{
    const QByteArray custom = qgetenv("XDG_DATA_HOME");
    const QString dataHome = custom.isEmpty()
        ? QDir::homePath() + QStringLiteral("/.local/share")
        : QString::fromLocal8Bit(custom);
    return dataHome + QStringLiteral("/Trash");
}

QString uniqueTrashName(const QString &filesDir, const QString &name)
{
    if (!QFileInfo::exists(QDir(filesDir).filePath(name)))
        return name;

    const QFileInfo info(name);
    const QString base = info.completeBaseName().isEmpty() ? name : info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix();

    for (int i = 2; i < 10000; ++i) {
        const QString candidate = QStringLiteral("%1.%2%3").arg(base).arg(i).arg(suffix);
        if (!QFileInfo::exists(QDir(filesDir).filePath(candidate)))
            return candidate;
    }
    return name;
}

} // namespace

// Own implementation of the freedesktop trash specification instead of
// QFile::moveToTrash, because the Qt variant refuses to create a missing trash
// directory and leaves the user with no way to delete anything.
bool moveToTrash(const QStringList &paths, QString *error)
{
    const QString base = trashBaseDir();
    const QString filesDir = base + QStringLiteral("/files");
    const QString infoDir = base + QStringLiteral("/info");

    if (!QDir().mkpath(filesDir) || !QDir().mkpath(infoDir)) {
        if (error)
            *error = QObject::tr("Could not create the trash folder in %1").arg(base);
        return false;
    }

    const Progress noProgress;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (!info.exists())
            continue;

        const QString trashName = uniqueTrashName(filesDir, info.fileName());
        const QString targetPath = QDir(filesDir).filePath(trashName);
        const QString infoPath = QDir(infoDir).filePath(trashName + QStringLiteral(".trashinfo"));

        QFile infoFile(infoPath);
        if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (error)
                *error = QObject::tr("Could not write %1").arg(infoPath);
            return false;
        }

        const QByteArray encodedPath = QUrl::toPercentEncoding(info.absoluteFilePath(), "/");
        infoFile.write("[Trash Info]\nPath=" + encodedPath + "\nDeletionDate="
                       + QDateTime::currentDateTime()
                             .toString(QStringLiteral("yyyy-MM-ddThh:mm:ss"))
                             .toUtf8()
                       + "\n");
        infoFile.close();

        if (QFile::rename(info.absoluteFilePath(), targetPath))
            continue;

        // Different filesystem: copy across, then remove the original.
        QString copyError;
        const bool copied = copyRecursively(info.absoluteFilePath(), targetPath, &copyError,
                                            noProgress);
        const bool removed = info.isDir() ? QDir(info.absoluteFilePath()).removeRecursively()
                                          : QFile::remove(info.absoluteFilePath());
        if (!copied || !removed) {
            QFile::remove(infoPath);
            if (error)
                *error = copyError.isEmpty()
                    ? QObject::tr("Could not move %1 to the trash").arg(info.fileName())
                    : copyError;
            return false;
        }
    }

    return true;
}

namespace {

// Deleting a large tree deserves the same progress and cancel as copying it.
bool removeTree(const QString &path, QString *error, const FileOps::Progress &progress)
{
    if (cancelledNow(progress))
        return false;

    const QFileInfo info(path);

    if (info.isSymLink() || !info.isDir()) {
        if (!QFile::remove(path)) {
            if (error)
                *error = QObject::tr("Could not delete %1").arg(info.fileName());
            return false;
        }
        if (progress.step)
            progress.step(info.size());
        if (progress.itemDone)
            progress.itemDone();
        return true;
    }

    if (progress.current)
        progress.current(info.fileName());

    const QFileInfoList children = QDir(path).entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &child : children) {
        if (!removeTree(child.absoluteFilePath(), error, progress))
            return false;
    }

    if (!QDir().rmdir(path)) {
        if (error)
            *error = QObject::tr("Could not delete the folder %1").arg(info.fileName());
        return false;
    }
    return true;
}

} // namespace

bool removePermanently(const QStringList &paths, QString *error, const Progress &progress)
{
    qint64 totalBytes = 0;
    for (const QString &path : paths)
        totalBytes += treeBytes(path);
    if (progress.total)
        progress.total(totalBytes);

    for (const QString &path : paths) {
        if (cancelled(progress))
            return false;
        if (!removeTree(path, error, progress))
            return false;
    }
    return true;
}

} // namespace FileOps
