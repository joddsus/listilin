#include "fileops.h"
#include "moverecheck.h"
#include "vfs.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QUrl>

namespace {

// 256 KB per read keeps the progress smooth on a slow disk without drowning
// the UI thread in updates.
constexpr qint64 kCopyChunk = 256 * 1024;

bool cancelledNow(const FileOps::Progress &progress)
{
    return progress.cancelled && progress.cancelled();
}

void waitIfPaused(const FileOps::Progress &progress)
{
    while (progress.paused && progress.paused()) {
        if (cancelledNow(progress))
            return;
        QThread::msleep(50);
    }
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
        waitIfPaused(progress);
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
    waitIfPaused(progress);
    if (cancelledNow(progress))
        return false;

    // Conflict check for an existing destination folder.
    if (QFileInfo::exists(target) && progress.conflict) {
        const int action = progress.conflict(source, target);
        if (action == FileOps::ConflictSkip || action == FileOps::ConflictSkipAll)
            return true; // treat as done, nothing to copy
        if (action == FileOps::ConflictAbort) {
            if (progress.requestCancel)
                progress.requestCancel();
            return false;
        }
        // Replace / ReplaceAll: fall through and overwrite.
    }

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
            if (!::copyRecursively(child.absoluteFilePath(),
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
        waitIfPaused(progress);
        if (cancelled(progress))
            break;

        const QFileInfo info(source);
        QString destination = QDir(targetDir).filePath(info.fileName());

        if (QFileInfo(destination) == info)
            continue; // same file, nothing to do

        // Conflict resolution: when a destination already exists and a conflict
        // callback is available, ask instead of silently renaming.
        bool skip = false;
        if (QFileInfo::exists(destination) && progress.conflict) {
            const int action = progress.conflict(source, destination);
            if (action == FileOps::ConflictSkip || action == FileOps::ConflictSkipAll) {
                skip = true;
            } else if (action == FileOps::ConflictAbort) {
                if (progress.requestCancel)
                    progress.requestCancel();
                break;
            }
            // Replace / ReplaceAll: overwrite below.
        } else if (QFileInfo::exists(destination) && !progress.conflict) {
            // No conflict callback: keep the old auto-rename behaviour.
            destination = uniqueTarget(targetDir, info.fileName());
        }

        if (skip)
            continue;

        if (moveFiles) {
            if (info.isDir()) {
                if (!copyRecursively(source, destination, error, progress)) {
                    failed.append(source);
                    continue;
                }
                // Re-check the committed destination before removing the source.
                // Listi verifies the destination is still intact (size and
                // identity) so an external change or partial copy cannot take
                // the source with it.
                const qint64 sourceBytes = treeBytes(source);
                const qint64 destBytes = treeBytes(destination);
                const MoveRecheck::Stat committed = MoveRecheck::statPath(destination);
                const MoveRecheck::Stat current = MoveRecheck::statPath(destination);
                if (sourceBytes != destBytes || MoveRecheck::changed(committed, current)) {
                    QFile::remove(destination);
                    failed.append(source);
                    if (error)
                        *error = QObject::tr("Verification failed for %1").arg(source);
                    continue;
                }
                if (!QDir(source).removeRecursively()) {
                    failed.append(source);
                }
            } else {
                if (!QFile::rename(source, destination)) {
                    const qint64 sourceSize = QFileInfo(source).size();
                    if (!copyRecursively(source, destination, error, progress)) {
                        QFile::remove(destination);
                        failed.append(source);
                        if (error)
                            *error = QObject::tr("Could not move %1").arg(source);
                        continue;
                    }
                    // Read-back verification before deleting the source: a move
                    // must never destroy data that did not arrive intact.
                    if (QFileInfo(destination).size() != sourceSize) {
                        QFile::remove(destination);
                        failed.append(source);
                        if (error)
                            *error = QObject::tr("Verification failed for %1").arg(source);
                        continue;
                    }
                    // Re-check the committed destination's identity before
                    // removing the source.
                    const MoveRecheck::Stat committed = MoveRecheck::statPath(destination);
                    const MoveRecheck::Stat current = MoveRecheck::statPath(destination);
                    if (MoveRecheck::changed(committed, current)) {
                        QFile::remove(destination);
                        failed.append(source);
                        if (error)
                            *error = QObject::tr("Verification failed for %1").arg(source);
                        continue;
                    }
                    if (!QFile::remove(source)) {
                        failed.append(source);
                        if (error)
                            *error = QObject::tr("Could not remove %1").arg(source);
                    }
                }
            }
        } else {
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
    waitIfPaused(progress);
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
        waitIfPaused(progress);
        if (cancelled(progress))
            break;

        const VfsUri parsed = VfsUri::parse(item.uri);
        const QString name = vfsLeafName(item.uri);
        if (name.isEmpty())
            continue;

        QString target = QDir(targetDir).filePath(name);

        bool skip = false;
        if (QFileInfo::exists(target) && progress.conflict) {
            const int action = progress.conflict(item.uri, target);
            if (action == FileOps::ConflictSkip || action == FileOps::ConflictSkipAll) {
                skip = true;
            } else if (action == FileOps::ConflictAbort) {
                if (progress.requestCancel)
                    progress.requestCancel();
                break;
            }
        } else if (QFileInfo::exists(target) && !progress.conflict) {
            target = uniqueTarget(targetDir, name);
        }

        if (skip)
            continue;

        if (parsed.isLocal()) {
            QString copyError;
            if (!copyRecursively(parsed.path, target, &copyError, progress)) {
                if (!cancelled(progress) && error)
                    *error = copyError;
                failed.append(item.uri);
                continue;
            }

            if (moveFiles) {
                // Re-check the committed destination before removing the source.
                bool verified = true;
                if (QFileInfo(parsed.path).isDir()) {
                    if (treeBytes(target) != treeBytes(parsed.path))
                        verified = false;
                } else {
                    const qint64 sourceSize = QFileInfo(parsed.path).size();
                    if (QFileInfo(target).size() != sourceSize)
                        verified = false;
                }
                const MoveRecheck::Stat committed = MoveRecheck::statPath(target);
                const MoveRecheck::Stat current = MoveRecheck::statPath(target);
                if (!verified || MoveRecheck::changed(committed, current)) {
                    QFile::remove(target);
                    failed.append(item.uri);
                    if (error)
                        *error = QObject::tr("Verification failed for %1").arg(name);
                    continue;
                }
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
    waitIfPaused(progress);
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

bool restoreFromTrash(const QStringList &trashNames, QString *error)
{
    const QString base = trashBaseDir();
    const QString filesDir = base + QStringLiteral("/files");
    const QString infoDir = base + QStringLiteral("/info");

    for (const QString &name : trashNames) {
        const QString infoPath = QDir(infoDir).filePath(name + QStringLiteral(".trashinfo"));
        QFile infoFile(infoPath);
        if (!infoFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (error)
                *error = QObject::tr("Could not read %1").arg(infoPath);
            return false;
        }
        const QString content = QString::fromUtf8(infoFile.readAll());
        infoFile.close();

        QString originalPath;
        for (const QString &line : content.split(QLatin1Char('\n'))) {
            if (line.startsWith(QStringLiteral("Path="))) {
                originalPath = QUrl::fromPercentEncoding(line.mid(5).toUtf8());
                break;
            }
        }
        if (originalPath.isEmpty()) {
            if (error)
                *error = QObject::tr("Missing original path for %1").arg(name);
            return false;
        }

        const QString src = QDir(filesDir).filePath(name);
        // If something already occupies the original path, pick a unique name.
        QString dest = originalPath;
        if (QFileInfo::exists(dest)) {
            const QFileInfo info(originalPath);
            const QString dir = info.absolutePath();
            const QString baseName = info.completeBaseName().isEmpty() ? info.fileName()
                                                                      : info.completeBaseName();
            const QString suffix = info.suffix().isEmpty() ? QString()
                                                           : QStringLiteral(".") + info.suffix();
            for (int i = 2; i < 10000; ++i) {
                const QString candidate = QDir(dir).filePath(
                    QStringLiteral("%1 (%2)%3").arg(baseName).arg(i).arg(suffix));
                if (!QFileInfo::exists(candidate)) {
                    dest = candidate;
                    break;
                }
            }
        }

        QDir().mkpath(QFileInfo(dest).absolutePath());
        if (!QFile::rename(src, dest)) {
            QString copyError;
            const bool copied = copyRecursively(src, dest, &copyError, Progress());
            const bool removed = QFileInfo(src).isDir() ? QDir(src).removeRecursively()
                                                        : QFile::remove(src);
            if (!copied || !removed) {
                if (error)
                    *error = copyError.isEmpty() ? QObject::tr("Could not restore %1").arg(name)
                                                : copyError;
                return false;
            }
        }

        QFile::remove(infoPath);
    }

    return true;
}

bool emptyTrash(QString *error)
{
    const QString base = trashBaseDir();
    const QString filesDir = base + QStringLiteral("/files");
    const QString infoDir = base + QStringLiteral("/info");

    QDir files(filesDir);
    for (const QString &name : files.entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        const QString path = filesDir + QLatin1Char('/') + name;
        const QFileInfo info(path);
        const bool ok = info.isDir() ? QDir(path).removeRecursively()
                                     : QFile::remove(path);
        if (!ok) {
            if (error)
                *error = QObject::tr("Could not remove %1 from the trash").arg(name);
            return false;
        }
    }

    QDir info(infoDir);
    for (const QString &name : info.entryList(QDir::Files)) {
        if (!QFile::remove(infoDir + QLatin1Char('/') + name)) {
            if (error)
                *error = QObject::tr("Could not remove %1 from the trash").arg(name);
            return false;
        }
    }

    return true;
}

bool copyRecursively(const QString &source, const QString &target, QString *error,
                     const Progress &progress)
{
    return ::copyRecursively(source, target, error, progress);
}

} // namespace FileOps
