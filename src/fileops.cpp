#include "filehash.h"
#include "fileops.h"
#include "moverecheck.h"
#include "pathsafety.h"
#include "vfs.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QThread>
#include <QUuid>
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

    // Stage the copy in a private temporary sibling so an interrupted or
    // failed write can never destroy a pre-existing destination. The original
    // target is only replaced after the temporary file has been fully written,
    // closed and verified.
    const QString tempPath = QDir(QFileInfo(target).absolutePath())
                                 .filePath(QStringLiteral(".listista-copy-%1")
                                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (QFile::exists(tempPath))
        QFile::remove(tempPath);

    QFile out(tempPath);
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
    const bool flushed = out.flush();
    out.close();
    in.close();

    if (!ok || !flushed || out.error() != QFileDevice::NoError) {
        // Never leave half a file behind looking like a finished copy.
        QFile::remove(tempPath);
        if (error && error->isEmpty())
            *error = QObject::tr("Could not finalise %1").arg(target);
        return false;
    }

    if (cancelledNow(progress)) {
        QFile::remove(tempPath);
        return false;
    }

    // Verify the staged file before it is published. The existing target is
    // left untouched while this check runs, so a failed verification cannot
    // remove or replace data that was already there.
    QString hashError;
    const QString sourceDigest = FileHash::hexDigest(source, FileHash::Algorithm::Sha256, &hashError);
    const QString stagedDigest = FileHash::hexDigest(tempPath, FileHash::Algorithm::Sha256, &hashError);
    if (sourceDigest.isEmpty() || stagedDigest.isEmpty() || sourceDigest != stagedDigest) {
        QFile::remove(tempPath);
        if (error)
            *error = QObject::tr("Verification failed for %1").arg(QFileInfo(source).fileName());
        return false;
    }

    // Publish with a replacement that keeps the existing target in place until
    // the staged file has taken its name. If the final rename fails, restore
    // the original target instead of leaving the destination missing.
    const bool hadTarget = QFileInfo::exists(target);
    QString backupPath;
    if (hadTarget) {
        backupPath = QDir(QFileInfo(target).absolutePath())
                        .filePath(QStringLiteral(".listista-backup-%1")
                                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
        if (QFile::exists(backupPath))
            QFile::remove(backupPath);
        if (!QFile::rename(target, backupPath)) {
            QFile::remove(tempPath);
            if (error)
                *error = QObject::tr("Could not overwrite %1").arg(target);
            return false;
        }
    }

    if (!QFile::rename(tempPath, target)) {
        if (hadTarget)
            QFile::rename(backupPath, target);
        QFile::remove(tempPath);
        if (error)
            *error = QObject::tr("Could not finalise %1").arg(target);
        return false;
    }

    if (hadTarget)
        QFile::remove(backupPath);

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

QString uniqueTarget(const QString &targetDir, const QString &name);

FileOps::CopyStatus copyRecursively(const QString &source, const QString &target, QString *error,
                     const FileOps::Progress &progress)
{
    QString destination = target;
    waitIfPaused(progress);
    if (cancelledNow(progress))
        return FileOps::CopyAborted;

    // Conflict check for an existing destination.
    if (QFileInfo::exists(target) && progress.conflict) {
        const int action = progress.conflict(source, target);
        if (action == FileOps::ConflictSkip || action == FileOps::ConflictSkipAll)
            return FileOps::CopySkipped;
        if (action == FileOps::ConflictAbort) {
            if (progress.requestCancel)
                progress.requestCancel();
            return FileOps::CopyAborted;
        }
        if (action == FileOps::ConflictKeepBoth || action == FileOps::ConflictKeepBothAll) {
            destination = uniqueTarget(QFileInfo(target).absolutePath(), QFileInfo(target).fileName());
        }
        // Replace / ReplaceAll: fall through and overwrite.
    }

    const QFileInfo info(source);

    // A symlink is copied as a symlink: following it would duplicate data and
    // could loop forever on a directory that points at its own parent.
    if (info.isSymLink()) {
        if (QFileInfo::exists(destination) && !QFile::remove(destination)) {
            if (error)
                *error = QObject::tr("Could not overwrite %1").arg(destination);
            return FileOps::CopyFailed;
        }
        if (!QFile::link(info.symLinkTarget(), destination)) {
            if (error)
                *error = QObject::tr("Could not create the link %1").arg(destination);
            return FileOps::CopyFailed;
        }
        if (progress.itemDone)
            progress.itemDone();
        return FileOps::CopyOk;
    }

    if (info.isDir()) {
        QDir dir(destination);
        if (!dir.exists() && !QDir().mkpath(destination)) {
            if (error)
                *error = QObject::tr("Could not create %1").arg(destination);
            return FileOps::CopyFailed;
        }

        if (progress.current)
            progress.current(info.fileName());

        FileOps::CopyStatus aggregate = FileOps::CopyOk;
        const QFileInfoList children
            = QDir(source).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const QFileInfo &child : children) {
            if (cancelledNow(progress))
                return FileOps::CopyAborted;
            const FileOps::CopyStatus status = ::copyRecursively(child.absoluteFilePath(),
                                 QDir(destination).filePath(child.fileName()), error, progress);
            if (status == FileOps::CopyFailed) {
                aggregate = FileOps::CopyFailed;
                break;
            }
            if (status == FileOps::CopyAborted) {
                aggregate = FileOps::CopyAborted;
                break;
            }
            if (status == FileOps::CopySkipped && aggregate == FileOps::CopyOk)
                aggregate = FileOps::CopySkipped;
        }
        if (cancelledNow(progress))
            return FileOps::CopyAborted;
        return aggregate;
    }

    if (progress.current)
        progress.current(info.fileName());

    if (!copyFileData(source, destination, error, progress)) {
        if (cancelledNow(progress))
            return FileOps::CopyAborted;
        return FileOps::CopyFailed;
    }

    if (progress.itemDone)
        progress.itemDone();
    return FileOps::CopyOk;
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

FileOps::Result copyAllResult(const QStringList &sources, const QString &targetDir, bool moveFiles,
                             QString *error, const Progress &progress)
{
    FileOps::Result result;
    result.itemsTotal = sources.size();

    if (!QFileInfo(targetDir).isDir()) {
        if (error)
            *error = QObject::tr("%1 is not a folder").arg(targetDir);
        result.failed = sources;
        result.error = error ? *error : QString();
        return result;
    }

    qint64 totalBytes = 0;
    for (const QString &source : sources)
        totalBytes += treeBytes(source);
    result.bytesTotal = totalBytes;

    Progress wrapped = progress;
    if (progress.total)
        wrapped.total = [&result, progress](qint64 totalBytes) {
            result.bytesTotal = totalBytes;
            progress.total(totalBytes);
        };
    if (progress.step)
        wrapped.step = [&result, progress](qint64 bytes) {
            result.bytesCopied += bytes;
            progress.step(bytes);
        };
    if (progress.itemDone)
        wrapped.itemDone = [&result, progress]() {
            ++result.itemsCopied;
            progress.itemDone();
        };
    if (progress.current)
        wrapped.current = progress.current;
    if (progress.cancelled)
        wrapped.cancelled = progress.cancelled;
    if (progress.paused)
        wrapped.paused = progress.paused;
    if (progress.requestCancel)
        wrapped.requestCancel = progress.requestCancel;
    if (progress.conflict)
        wrapped.conflict = progress.conflict;

    if (wrapped.total)
        wrapped.total(totalBytes);

    for (const QString &source : sources) {
        waitIfPaused(wrapped);
        if (cancelled(wrapped))
            break;

        const QFileInfo info(source);
        QString destination = QDir(targetDir).filePath(info.fileName());

        if (QFileInfo(destination) == info) {
            ++result.itemsCopied;
            continue; // same file, nothing to do
        }

        // Conflict resolution: when a destination already exists and a conflict
        // callback is available, ask instead of silently renaming.
        bool skip = false;
        if (QFileInfo::exists(destination) && wrapped.conflict) {
            const int action = wrapped.conflict(source, destination);
            if (action == FileOps::ConflictSkip || action == FileOps::ConflictSkipAll) {
                skip = true;
            } else if (action == FileOps::ConflictAbort) {
                if (wrapped.requestCancel)
                    wrapped.requestCancel();
                break;
            } else if (action == FileOps::ConflictKeepBoth || action == FileOps::ConflictKeepBothAll) {
                destination = uniqueTarget(targetDir, info.fileName());
            }
            // Replace / ReplaceAll: overwrite below.
        } else if (QFileInfo::exists(destination) && !wrapped.conflict) {
            // No conflict callback: keep the old auto-rename behaviour.
            destination = uniqueTarget(targetDir, info.fileName());
        }

        if (skip) {
            ++result.itemsSkipped;
            ++result.skipped;
            continue;
        }

        if (moveFiles) {
            if (info.isDir()) {
                const FileOps::CopyStatus recursiveStatus =
                    copyRecursively(source, destination, error, wrapped);
                if (recursiveStatus != FileOps::CopyOk) {
                    if (recursiveStatus == FileOps::CopyFailed)
                        result.failed.append(source);
                    if (recursiveStatus == FileOps::CopySkipped) {
                        ++result.itemsSkipped;
                        ++result.skipped;
                    }
                    if (recursiveStatus == FileOps::CopyAborted)
                        result.cancelled = true;
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
                    result.failed.append(source);
                    if (error)
                        *error = QObject::tr("Verification failed for %1").arg(source);
                    continue;
                }
                if (!QDir(source).removeRecursively()) {
                    result.failed.append(source);
                } else {
                    ++result.itemsCopied;
                }
            } else {
                if (!QFile::rename(source, destination)) {
                    const qint64 sourceSize = QFileInfo(source).size();
                    const FileOps::CopyStatus moveStatus =
                        copyRecursively(source, destination, error, wrapped);
                    if (moveStatus != FileOps::CopyOk) {
                        if (moveStatus == FileOps::CopyFailed) {
                            QFile::remove(destination);
                            result.failed.append(source);
                            if (error)
                                *error = QObject::tr("Could not move %1").arg(source);
                        }
                        if (moveStatus == FileOps::CopySkipped) {
                            ++result.itemsSkipped;
                            ++result.skipped;
                        }
                        if (moveStatus == FileOps::CopyAborted)
                            result.cancelled = true;
                        continue;
                    }
                    // Read-back verification before deleting the source: a move
                    // must never destroy data that did not arrive intact.
                    if (QFileInfo(destination).size() != sourceSize) {
                        QFile::remove(destination);
                        result.failed.append(source);
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
                        result.failed.append(source);
                        if (error)
                            *error = QObject::tr("Verification failed for %1").arg(source);
                        continue;
                    }
                    if (!QFile::remove(source)) {
                        result.failed.append(source);
                        if (error)
                            *error = QObject::tr("Could not remove %1").arg(source);
                    } else {
                        ++result.itemsCopied;
                    }
                } else {
                    ++result.itemsCopied;
                }
            }
        } else {
            const FileOps::CopyStatus copyStatus =
                copyRecursively(source, destination, error, wrapped);
            if (copyStatus == FileOps::CopyOk)
                ++result.itemsCopied;
            else if (copyStatus == FileOps::CopySkipped) {
                ++result.itemsSkipped;
                ++result.skipped;
            } else if (copyStatus == FileOps::CopyAborted) {
                result.cancelled = true;
            } else {
                result.failed.append(source);
            }
        }
    }

    result.cancelled = wrapped.cancelled && wrapped.cancelled();
    result.failed = result.failed;
    result.error = error ? *error : QString();
    if (result.itemsTotal == 0)
        result.itemsTotal = result.itemsCopied + result.failed.size();
    return result;
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

        // Defend against a malicious or malformed listing: only join a single,
        // safe component and never let the destination escape `target`.
        if (!PathSafety::isSafePathComponent(entry.name))
            continue;
        QString joinError;
        const QString childTarget = PathSafety::safeJoin(target, entry.name, &joinError);
        if (childTarget.isEmpty()) {
            if (error)
                *error = joinError;
            continue;
        }

        const QString childUri = VfsUri::childUri(uri, entry.name);

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

FileOps::Result copyUrisResult(const QVector<CopyItem> &items, const QString &targetDir, bool moveFiles,
                               QString *error, const Progress &progress)
{
    FileOps::Result result;

    if (!QFileInfo(targetDir).isDir()) {
        if (error)
            *error = QObject::tr("%1 is not a folder").arg(targetDir);
        for (const CopyItem &item : items)
            result.failed.append(item.uri);
        result.error = error ? *error : QString();
        return result;
    }

    qint64 totalBytes = 0;
    for (const CopyItem &item : items) {
        const VfsUri parsed = VfsUri::parse(item.uri);
        if (parsed.isLocal())
            totalBytes += treeBytes(parsed.path);
    }
    result.bytesTotal = totalBytes;
    result.itemsTotal = items.size();

    Progress wrapped = progress;
    if (progress.total)
        wrapped.total = [&result, progress](qint64 totalBytes) {
            result.bytesTotal = totalBytes;
            progress.total(totalBytes);
        };
    if (progress.step)
        wrapped.step = [&result, progress](qint64 bytes) {
            result.bytesCopied += bytes;
            progress.step(bytes);
        };
    if (progress.itemDone)
        wrapped.itemDone = [&result, progress]() {
            ++result.itemsCopied;
            progress.itemDone();
        };
    if (wrapped.current) wrapped.current = wrapped.current;
    if (progress.cancelled) wrapped.cancelled = progress.cancelled;
    if (progress.paused) wrapped.paused = progress.paused;
    if (wrapped.requestCancel) wrapped.requestCancel = wrapped.requestCancel;
    if (wrapped.conflict) wrapped.conflict = wrapped.conflict;

    if (wrapped.total)
        wrapped.total(totalBytes);

    for (const CopyItem &item : items) {
        waitIfPaused(wrapped);
        if (cancelled(wrapped))
            break;

        const VfsUri parsed = VfsUri::parse(item.uri);
        const QString name = vfsLeafName(item.uri);
        if (name.isEmpty())
            continue;

        // Defend against a crafted URI whose leaf name is not a safe single
        // component (path traversal, reserved device name, control chars).
        if (!PathSafety::isSafePathComponent(name)) {
            result.failed.append(item.uri);
            if (error)
                *error = QStringLiteral("Unsafe file name: %1").arg(name);
            continue;
        }

        QString target = PathSafety::safeJoin(targetDir, name);
        if (target.isEmpty()) {
            result.failed.append(item.uri);
            if (error)
                *error = QStringLiteral("Unsafe destination: %1").arg(name);
            continue;
        }

        bool skip = false;
        if (QFileInfo::exists(target) && wrapped.conflict) {
            const int action = wrapped.conflict(item.uri, target);
            if (action == FileOps::ConflictSkip || action == FileOps::ConflictSkipAll) {
                skip = true;
            } else if (action == FileOps::ConflictAbort) {
                if (wrapped.requestCancel)
                    wrapped.requestCancel();
                break;
            } else if (action == FileOps::ConflictKeepBoth || action == FileOps::ConflictKeepBothAll) {
                target = uniqueTarget(targetDir, name);
            }
        } else if (QFileInfo::exists(target) && !wrapped.conflict) {
            target = uniqueTarget(targetDir, name);
        }

        if (skip) {
            ++result.itemsSkipped;
            ++result.skipped;
            continue;
        }

        if (parsed.isLocal()) {
            QString copyError;
            const FileOps::CopyStatus localStatus =
                copyRecursively(parsed.path, target, &copyError, wrapped);
            if (localStatus != FileOps::CopyOk) {
                if (localStatus == FileOps::CopyFailed) {
                    if (!cancelled(wrapped) && error)
                        *error = copyError;
                    result.failed.append(item.uri);
                }
                if (localStatus == FileOps::CopySkipped) {
                    ++result.itemsSkipped;
                    ++result.skipped;
                }
                if (localStatus == FileOps::CopyAborted)
                    result.cancelled = true;
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
                    result.failed.append(item.uri);
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
                    result.failed.append(item.uri);
                } else {
                    ++result.itemsCopied;
                }
            } else {
                ++result.itemsCopied;
            }
            continue;
        }

        // Archive and remote entries are pulled out through their backend.
        if (wrapped.current)
            wrapped.current(name);

        QString fetchError;
        const bool ok = item.isDir ? copyRemoteTree(item.uri, target, &fetchError, wrapped)
                                   : Vfs::instance().fetch(item.uri, target, &fetchError);
        if (!ok) {
            if (!cancelled(wrapped) && error)
                *error = fetchError;
            result.failed.append(item.uri);
        } else {
            ++result.itemsCopied;
        }
    }

    result.cancelled = wrapped.cancelled && wrapped.cancelled();
    result.error = error ? *error : QString();
    if (result.itemsTotal == 0)
        result.itemsTotal = result.itemsCopied + result.failed.size();
    return result;
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
bool moveToTrash(const QStringList &paths, QStringList *trashNames, QString *error)
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

        if (QFile::rename(info.absoluteFilePath(), targetPath)) {
            if (trashNames)
                trashNames->append(trashName);
            continue;
        }

        // Different filesystem: copy across, then remove the original. The
        // original is removed only after the copy has been independently
        // verified; a failed or cancelled copy must never take the source.
        QString copyError;
        const FileOps::CopyStatus copied = copyRecursively(info.absoluteFilePath(), targetPath, &copyError,
                                            noProgress);
        if (copied != FileOps::CopyOk) {
            QFile::remove(infoPath);
            QFile::remove(targetPath);
            if (error)
                *error = copyError.isEmpty()
                    ? QObject::tr("Could not move %1 to the trash").arg(info.fileName())
                    : copyError;
            return false;
        }

        // Verify the destination before touching the source.
        const qint64 sourceBytes = treeBytes(info.absoluteFilePath());
        const qint64 destBytes = treeBytes(targetPath);
        if (sourceBytes != destBytes) {
            QFile::remove(infoPath);
            QFile::remove(targetPath);
            if (error)
                *error = QObject::tr("Verification failed for %1").arg(info.fileName());
            return false;
        }

        const bool removed = info.isDir() ? QDir(info.absoluteFilePath()).removeRecursively()
                                          : QFile::remove(info.absoluteFilePath());
        if (removed && trashNames)
            trashNames->append(trashName);
        if (!removed) {
            // Keep the trash copy and the original; report the failure instead
            // of pretending the item was trashed.
            if (error)
                *error = QObject::tr("Could not remove %1 after copying it to the trash")
                             .arg(info.fileName());
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
            const FileOps::CopyStatus copied = copyRecursively(src, dest, &copyError, Progress());
            if (copied != FileOps::CopyOk) {
                QFile::remove(dest);
                if (error)
                    *error = copyError.isEmpty() ? QObject::tr("Could not restore %1").arg(name)
                                                : copyError;
                return false;
            }
            if (treeBytes(src) != treeBytes(dest)) {
                QFile::remove(dest);
                if (error)
                    *error = QObject::tr("Verification failed for %1").arg(name);
                return false;
            }
            const bool removed = QFileInfo(src).isDir() ? QDir(src).removeRecursively()
                                                        : QFile::remove(src);
            if (!removed) {
                if (error)
                    *error = QObject::tr("Could not remove %1 from the trash after restoring it")
                                 .arg(name);
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

FileOps::CopyStatus copyRecursively(const QString &source, const QString &target, QString *error,
                     const Progress &progress)
{
    return ::copyRecursively(source, target, error, progress);
}

QStringList copyAll(const QStringList &sources, const QString &targetDir, bool moveFiles,
                    QString *error, const Progress &progress)
{
    return copyAllResult(sources, targetDir, moveFiles, error, progress).failed;
}

QStringList copyUris(const QVector<CopyItem> &items, const QString &targetDir, bool moveFiles,
                     QString *error, const Progress &progress)
{
    return copyUrisResult(items, targetDir, moveFiles, error, progress).failed;
}

} // namespace FileOps
