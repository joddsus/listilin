#include "filehash.h"
#include "fileops.h"
#include "atomicfile.h"
#include "pathsafety.h"
#include "transferplan.h"
#include "treesnapshot.h"
#include "vfs.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryFile>
#include <QThread>
#include <QUrl>

#include <algorithm>
#include <memory>

#ifdef Q_OS_UNIX
#include <sys/xattr.h>
#endif

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

// Metadata that a copy has to carry over. Only the portable "user." extended
// attributes are copied; system and security namespaces belong to the system.
struct FileMetadata
{
    QFile::Permissions permissions;
    QDateTime modified;
    QDateTime accessed;
    QVector<QPair<QByteArray, QByteArray>> xattrs;
};

QVector<QPair<QByteArray, QByteArray>> readUserXattrs(const QString &path)
{
    QVector<QPair<QByteArray, QByteArray>> result;
#ifdef Q_OS_UNIX
    const QByteArray native = QFile::encodeName(path);
    const ssize_t size = ::llistxattr(native.constData(), nullptr, 0);
    if (size <= 0)
        return result;

    QByteArray names;
    names.resize(int(size));
    const ssize_t read = ::llistxattr(native.constData(), names.data(), size_t(size));
    if (read <= 0)
        return result;

    int offset = 0;
    while (offset < read) {
        const QByteArray name = names.mid(offset, int(qstrlen(names.constData() + offset)));
        offset += name.size() + 1;
        if (!name.startsWith("user."))
            continue;

        const ssize_t valueSize = ::lgetxattr(native.constData(), name.constData(), nullptr, 0);
        if (valueSize < 0)
            continue;
        QByteArray value;
        value.resize(int(valueSize));
        if (valueSize > 0) {
            if (::lgetxattr(native.constData(), name.constData(), value.data(), size_t(valueSize))
                != valueSize) {
                continue;
            }
        }
        result.append(qMakePair(name, value));
    }
#else
    Q_UNUSED(path)
#endif
    return result;
}

FileMetadata readMetadata(const QString &path)
{
    FileMetadata metadata;
    const QFileInfo info(path);
    metadata.permissions = info.permissions();
    metadata.modified = info.lastModified();
    metadata.accessed = info.lastRead();
    metadata.xattrs = readUserXattrs(path);
    return metadata;
}

void applyMetadata(const QString &path, const FileMetadata &metadata)
{
    // Attributes first: they need a writable file, which may stop being the
    // case once a read-only mode has been applied.
    for (const auto &attribute : metadata.xattrs) {
#ifdef Q_OS_UNIX
        ::lsetxattr(QFile::encodeName(path).constData(), attribute.first.constData(),
                    attribute.second.constData(), size_t(attribute.second.size()), 0);
#endif
    }

    QFile device(path);
    if (device.open(QIODevice::ReadWrite)) {
        if (metadata.modified.isValid())
            device.setFileTime(metadata.modified, QFileDevice::FileModificationTime);
        if (metadata.accessed.isValid())
            device.setFileTime(metadata.accessed, QFileDevice::FileAccessTime);
        device.close();
    }

    QFile::setPermissions(path, metadata.permissions);
}

// Differences in the metadata a copy is expected to preserve. Directories are
// only compared on their extended attributes: a folder that already existed is
// deliberately merged into, and taking over its mode or timestamp would change
// the user's own folder.
QStringList metadataDifferences(const QString &source, const QString &target)
{
    const QFileInfo sourceInfo(source);
    if (sourceInfo.isSymLink())
        return QStringList();

    QStringList differences;
    const bool isDirectory = sourceInfo.isDir();

    const QVector<QPair<QByteArray, QByteArray>> sourceAttributes = readUserXattrs(source);
    const QVector<QPair<QByteArray, QByteArray>> targetAttributes = readUserXattrs(target);
    QHash<QByteArray, QByteArray> targetMap;
    for (const auto &attribute : targetAttributes)
        targetMap.insert(attribute.first, attribute.second);
    for (const auto &attribute : sourceAttributes) {
        if (!targetMap.contains(attribute.first) || targetMap.value(attribute.first) != attribute.second) {
            differences.append(QObject::tr("the metadata \"%1\" was not preserved on %2")
                                   .arg(QString::fromUtf8(attribute.first),
                                        QFileInfo(target).fileName()));
        }
    }

    if (!isDirectory) {
        const QFileInfo targetInfo(target);
        if ((sourceInfo.permissions() & QFileDevice::ReadOwner) != 0
            && targetInfo.permissions() != sourceInfo.permissions()) {
            differences.append(QObject::tr("the permissions of %1 were not preserved")
                                   .arg(QFileInfo(target).fileName()));
        }
        if (sourceInfo.lastModified().isValid() && targetInfo.lastModified().isValid()
            && sourceInfo.lastModified().toSecsSinceEpoch()
                   != targetInfo.lastModified().toSecsSinceEpoch()) {
            differences.append(QObject::tr("the timestamp of %1 was not preserved")
                                   .arg(QFileInfo(target).fileName()));
        }
    }

    return differences;
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
    // failed write can never destroy a pre-existing destination.
    const QString tempPath = AtomicFile::stagingPath(target, QStringLiteral("copy"));

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

    applyMetadata(tempPath, readMetadata(source));

    // Atomic replacement: the old target stays in place until the staged file
    // has taken its name, and the name is never missing in between.
    if (!AtomicFile::publish(tempPath, target, error)) {
        QFile::remove(tempPath);
        return false;
    }

    return true;
}

QString uniqueTarget(const QString &targetDir, const QString &name);

// Removing an entry never walks into a symlink: the link is the thing the
// user selected, not the folder it names.
bool removeTreeLocal(const QString &path, QString *error, const FileOps::Progress &progress)
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
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
    for (const QFileInfo &child : children) {
        if (!removeTreeLocal(child.absoluteFilePath(), error, progress))
            return false;
    }

    if (!QDir().rmdir(path)) {
        if (error)
            *error = QObject::tr("Could not delete the folder %1").arg(info.fileName());
        return false;
    }
    return true;
}

FileOps::CopyStatus copyRecursivelyImpl(const QString &source, const QString &target, QString *error,
                                        const FileOps::Progress &progress, bool conflictResolved)
{
    QString destination = target;
    waitIfPaused(progress);
    if (cancelledNow(progress))
        return FileOps::CopyAborted;

    const QFileInfo info(source);

    // Conflict check for an existing destination. A top-level conflict has
    // already been answered by the caller, so it must not ask twice.
    if (!conflictResolved && !TransferPlan::samePath(source, target) && QFileInfo::exists(target)
        && progress.conflict) {
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

    // A symlink is copied as a symlink: following it would duplicate data and
    // could loop forever on a directory that points at its own parent.
    if (info.isSymLink()) {
        if (QFileInfo::exists(destination) && !QFile::remove(destination)) {
            if (error)
                *error = QObject::tr("Could not overwrite %1").arg(destination);
            return FileOps::CopyFailed;
        }
        if (!QFile::link(TreeSnapshot::readLinkTarget(source), destination)) {
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
        const bool created = !dir.exists();
        if (created && !QDir().mkpath(destination)) {
            if (error)
                *error = QObject::tr("Could not create %1").arg(destination);
            return FileOps::CopyFailed;
        }

        if (progress.current)
            progress.current(info.fileName());

        FileOps::CopyStatus aggregate = FileOps::CopyOk;
        const QFileInfoList children
            = QDir(source).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden
                                            | QDir::System,
                                        QDir::Name);
        for (const QFileInfo &child : children) {
            if (cancelledNow(progress))
                return FileOps::CopyAborted;
            const FileOps::CopyStatus status = copyRecursivelyImpl(
                child.absoluteFilePath(), QDir(destination).filePath(child.fileName()), error,
                progress, false);
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

        // A folder that this copy created takes over the source's metadata;
        // an existing folder is merged into and keeps its own.
        if (created && aggregate == FileOps::CopyOk)
            applyMetadata(destination, readMetadata(source));

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

// Moves one entry without ever destroying the source before the destination has
// been proved to be a complete copy of it.
FileOps::CopyStatus moveLocalEntry(const QString &source, const QString &destination,
                                   bool destinationExisted, QString *error,
                                   const FileOps::Progress &progress)
{
    const QFileInfo info(source);

    // On the same volume a real rename keeps contents, timestamps, extended
    // attributes, hard links and symlinks exactly as they were, and it cannot
    // half-finish. A folder that already exists is merged instead, per Listi's
    // behaviour.
    if (!destinationExisted) {
        if (QFile::rename(source, destination)) {
            if (progress.itemDone)
                progress.itemDone();
            return FileOps::CopyOk;
        }
    }

    const TreeSnapshot::Tree before = TreeSnapshot::capture(source);
    if (!before.complete) {
        if (error)
            *error = before.error;
        return FileOps::CopyFailed;
    }

    const FileOps::CopyStatus copied
        = copyRecursivelyImpl(source, destination, error, progress, true);
    if (copied != FileOps::CopyOk)
        return copied;

    const TreeSnapshot::Tree after = TreeSnapshot::capture(destination);
    const QStringList absent = TreeSnapshot::missing(before, after);
    if (!absent.isEmpty()) {
        if (error) {
            *error = QObject::tr("The move did not deliver everything to %1: %2")
                         .arg(destination, absent.mid(0, 4).join(QStringLiteral("; ")));
        }
        return FileOps::CopyFailed;
    }

    const QStringList metadata = metadataDifferences(source, destination);
    if (!metadata.isEmpty()) {
        if (error) {
            *error = QObject::tr("The move could not preserve all of %1's metadata: %2")
                         .arg(info.fileName(), metadata.mid(0, 4).join(QStringLiteral("; ")));
        }
        return FileOps::CopyFailed;
    }

    // If the source changed while it was being copied, the copy is not the same
    // file any more; keep the source rather than publish a stale version.
    const TreeSnapshot::Tree current = TreeSnapshot::capture(source);
    const QStringList changed = TreeSnapshot::compare(before, current);
    if (!changed.isEmpty()) {
        if (error) {
            *error = QObject::tr("%1 changed while it was being moved; the original was kept.")
                         .arg(info.fileName());
        }
        return FileOps::CopyFailed;
    }

    if (!removeTreeLocal(source, error, progress))
        return FileOps::CopyFailed;

    return FileOps::CopyOk;
}

QString uniqueTarget(const QString &targetDir, const QString &name)
{
    QString candidate = QDir(targetDir).filePath(name);
    if (!QFileInfo::exists(candidate) && !QFileInfo(candidate).isSymLink())
        return candidate;

    const QFileInfo info(name);
    const QString base = info.completeBaseName().isEmpty() ? name : info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix();

    for (int i = 2; i < 10000; ++i) {
        candidate = QDir(targetDir).filePath(QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(suffix));
        if (!QFileInfo::exists(candidate) && !QFileInfo(candidate).isSymLink())
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

    // Refuse a job that would overwrite its own source before anything is
    // written. This is the check that keeps "Move here" in the same folder from
    // deleting the only copy of a file.
    const QStringList problems = TransferPlan::validate(sources, targetDir, moveFiles);
    if (!problems.isEmpty()) {
        result.failed = sources;
        result.error = problems.join(QLatin1Char('\n'));
        if (error)
            *error = result.error;
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

        // Moving or copying an entry onto itself leaves the original alone.
        if (TransferPlan::samePath(source, destination)) {
            ++result.itemsCopied;
            continue;
        }

        bool destinationExisted
            = QFileInfo::exists(destination) || QFileInfo(destination).isSymLink();
        bool skip = false;

        // Conflict resolution: when a destination already exists and a conflict
        // callback is available, ask instead of silently renaming.
        if (destinationExisted && wrapped.conflict) {
            const int action = wrapped.conflict(source, destination);
            if (action == FileOps::ConflictSkip || action == FileOps::ConflictSkipAll) {
                skip = true;
            } else if (action == FileOps::ConflictAbort) {
                if (wrapped.requestCancel)
                    wrapped.requestCancel();
                break;
            } else if (action == FileOps::ConflictKeepBoth || action == FileOps::ConflictKeepBothAll) {
                destination = uniqueTarget(targetDir, info.fileName());
                destinationExisted = false;
            }
            // Replace / ReplaceAll: overwrite below.
        } else if (destinationExisted && !wrapped.conflict) {
            // No conflict callback: keep the old auto-rename behaviour.
            destination = uniqueTarget(targetDir, info.fileName());
            destinationExisted = false;
        }

        if (skip) {
            ++result.itemsSkipped;
            ++result.skipped;
            continue;
        }

        const FileOps::CopyStatus status
            = moveFiles ? moveLocalEntry(source, destination, destinationExisted, error, wrapped)
                        : copyRecursivelyImpl(source, destination, error, wrapped, true);

        if (status == FileOps::CopyOk) {
            if (!moveFiles)
                ++result.itemsCopied;
            continue;
        }
        if (status == FileOps::CopySkipped) {
            ++result.itemsSkipped;
            ++result.skipped;
        } else if (status == FileOps::CopyAborted) {
            result.cancelled = true;
        } else {
            result.failed.append(source);
        }
    }

    result.cancelled = wrapped.cancelled && wrapped.cancelled();
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

// Every archive that a job reads from must survive the job untouched. A member
// named like the archive itself, copied into the archive's own folder with
// "Replace", would otherwise overwrite the whole container.
QStringList archiveProtectionProblems(const QVector<FileOps::CopyItem> &items,
                                      const QString &targetDir)
{
    QStringList problems;
    for (const FileOps::CopyItem &item : items) {
        const VfsUri parsed = VfsUri::parse(item.uri);
        if (!parsed.isArchive())
            continue;

        const QString name = vfsLeafName(item.uri);
        if (name.isEmpty())
            continue;
        const QString target = QDir(targetDir).filePath(name);
        if (TransferPlan::samePath(target, parsed.path)) {
            problems.append(QObject::tr("\"%1\" is the archive being read; it cannot be "
                                        "replaced by one of its own members.")
                                .arg(QFileInfo(parsed.path).fileName()));
        }
    }
    return problems;
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

    QStringList localSources;
    for (const CopyItem &item : items) {
        if (VfsUri::parse(item.uri).isLocal())
            localSources.append(VfsUri::parse(item.uri).path);
    }

    QStringList problems = TransferPlan::validate(localSources, targetDir, moveFiles);
    problems += archiveProtectionProblems(items, targetDir);
    if (!problems.isEmpty()) {
        for (const CopyItem &item : items)
            result.failed.append(item.uri);
        result.error = problems.join(QLatin1Char('\n'));
        if (error)
            *error = result.error;
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

        if (parsed.isLocal() && TransferPlan::samePath(parsed.path, target)) {
            ++result.itemsCopied;
            continue;
        }

        bool destinationExisted = QFileInfo::exists(target) || QFileInfo(target).isSymLink();
        bool skip = false;
        if (destinationExisted && wrapped.conflict) {
            const int action = wrapped.conflict(item.uri, target);
            if (action == FileOps::ConflictSkip || action == FileOps::ConflictSkipAll) {
                skip = true;
            } else if (action == FileOps::ConflictAbort) {
                if (wrapped.requestCancel)
                    wrapped.requestCancel();
                break;
            } else if (action == FileOps::ConflictKeepBoth || action == FileOps::ConflictKeepBothAll) {
                target = uniqueTarget(targetDir, name);
                destinationExisted = false;
            }
        } else if (destinationExisted && !wrapped.conflict) {
            target = uniqueTarget(targetDir, name);
            destinationExisted = false;
        }

        if (skip) {
            ++result.itemsSkipped;
            ++result.skipped;
            continue;
        }

        if (parsed.isLocal()) {
            const FileOps::CopyStatus localStatus
                = moveFiles
                      ? moveLocalEntry(parsed.path, target, destinationExisted, error, wrapped)
                      : copyRecursivelyImpl(parsed.path, target, error, wrapped, true);
            if (localStatus == FileOps::CopyOk) {
                if (!moveFiles)
                    ++result.itemsCopied;
                continue;
            }
            if (localStatus == FileOps::CopyFailed) {
                if (!cancelled(wrapped) && error)
                    *error = error->isEmpty() ? QObject::tr("Could not copy %1").arg(name) : *error;
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
            continue;
        }

        ++result.itemsCopied;
        if (moveFiles) {
            // The local copy is complete, so the second half of a move is to
            // remove the original. A backend that refuses (read-only format, a
            // server without delete) leaves both copies and says so instead of
            // reporting a finished move.
            QString removeError;
            if (!Vfs::instance().remove(item.uri, true, &removeError)) {
                result.failed.append(item.uri);
                if (error) {
                    *error = QObject::tr("%1 was copied, but the original could not be removed: %2")
                                 .arg(name, removeError);
                }
                continue;
            }
            UploadRecord::MoveOutItem record;
            record.localPath = target;
            record.sourceParentUri = VfsUri::parentUri(item.uri);
            record.sourceName = name;
            record.isDir = item.isDir;
            record.fingerprint = TreeSnapshot::fingerprint(target);
            result.movesOut.append(record);
        }
    }

    result.cancelled = wrapped.cancelled && wrapped.cancelled();
    result.error = error ? *error : QString();
    if (result.itemsTotal == 0)
        result.itemsTotal = result.itemsCopied + result.failed.size();
    return result;
}

namespace {

struct PreparedUndo
{
    UploadRecord::Item item;
    std::unique_ptr<QTemporaryFile> temp;
    bool needsRestore = false;
};

} // namespace

FileOps::Result undoUploadsResult(const QVector<UploadRecord::Item> &items, QString *error,
                                  const Progress &progress)
{
    FileOps::Result result;
    result.itemsTotal = items.size();

    // Read every uploaded file back and compare it with the digest recorded at
    // upload time before changing anything. A server that is gone, a member
    // that was edited by someone else and a missing original all stop the undo
    // with the uploaded copies still in place.
    std::vector<PreparedUndo> prepared;
    prepared.reserve(size_t(items.size()));
    for (const UploadRecord::Item &item : items) {
        if (item.isDir)
            continue;
        if (cancelled(progress)) {
            result.cancelled = true;
            if (error)
                *error = QObject::tr("Undo cancelled.");
            return result;
        }

        const QString name = vfsLeafName(item.destinationUri);
        auto temp = std::make_unique<QTemporaryFile>(
            QDir::tempPath() + QStringLiteral("/listista-undo-XXXXXX"));
        if (!temp->open()) {
            result.failed.append(item.destinationUri);
            if (error)
                *error = QObject::tr("Could not create a temporary file for the undo.");
            return result;
        }
        const QString tempPath = temp->fileName();
        temp->close();

        QString fetchError;
        if (!Vfs::instance().fetch(item.destinationUri, tempPath, &fetchError)) {
            result.failed.append(item.destinationUri);
            if (error) {
                *error = QObject::tr("%1 could not be read back, so nothing was changed: %2")
                             .arg(name, fetchError);
            }
            return result;
        }

        if (item.digest.isEmpty()) {
            result.failed.append(item.destinationUri);
            if (error)
                *error = QObject::tr("%1 has no verification record and will not be removed.")
                             .arg(name);
            return result;
        }

        QString hashError;
        const QString digest = FileHash::hexDigest(tempPath, FileHash::Algorithm::Sha256,
                                                   &hashError);
        if (digest.isEmpty() || digest != item.digest) {
            result.failed.append(item.destinationUri);
            if (error) {
                *error = QObject::tr("%1 changed after it was uploaded. It was left in place "
                                     "and the undo stopped.")
                             .arg(name);
            }
            return result;
        }

        PreparedUndo preparedItem;
        preparedItem.item = item;
        preparedItem.temp = std::move(temp);
        preparedItem.needsRestore = !item.sourcePath.isEmpty();

        if (preparedItem.needsRestore
            && (QFileInfo::exists(item.sourcePath) || QFileInfo(item.sourcePath).isSymLink())) {
            const QString localDigest = FileHash::hexDigest(
                item.sourcePath, FileHash::Algorithm::Sha256, nullptr);
            if (localDigest == item.digest) {
                // The file is already back (a previous, partly finished undo);
                // only the uploaded copy still has to go.
                preparedItem.needsRestore = false;
            } else {
                result.failed.append(item.destinationUri);
                if (error) {
                    *error = QObject::tr("%1 already exists again. It was not replaced and "
                                         "nothing was removed.")
                                 .arg(item.sourcePath);
                }
                return result;
            }
        }

        prepared.push_back(std::move(preparedItem));
    }

    // Recreate the original folders before their files. Sorting by path length
    // makes a nested folder come after its parent.
    QStringList sourceDirs;
    for (const UploadRecord::Item &item : items) {
        if (item.isDir && !item.sourcePath.isEmpty())
            sourceDirs.append(item.sourcePath);
    }
    std::sort(sourceDirs.begin(), sourceDirs.end(),
              [](const QString &a, const QString &b) { return a.size() < b.size(); });
    for (const QString &dir : sourceDirs) {
        if (!QDir().mkpath(dir)) {
            result.failed.append(dir);
            if (error)
                *error = QObject::tr("Could not recreate the folder %1.").arg(dir);
            return result;
        }
    }

    // Restore the local files through a staged sibling so an interrupted undo
    // never leaves a half-written file under the original name.
    for (PreparedUndo &entry : prepared) {
        if (!entry.needsRestore)
            continue;
        const QString parent = QFileInfo(entry.item.sourcePath).absolutePath();
        if (!QDir().mkpath(parent)) {
            result.failed.append(entry.item.destinationUri);
            if (error)
                *error = QObject::tr("Could not recreate the folder %1.").arg(parent);
            return result;
        }

        const QString staged = AtomicFile::stagingPath(entry.item.sourcePath,
                                                       QStringLiteral("undo"));
        QFile::remove(staged);
        if (!QFile::copy(entry.temp->fileName(), staged)) {
            result.failed.append(entry.item.destinationUri);
            if (error)
                *error = QObject::tr("Could not write %1 back.").arg(entry.item.sourcePath);
            return result;
        }
        QFile::setPermissions(staged, QFile::permissions(entry.temp->fileName()));
        if (!AtomicFile::publish(staged, entry.item.sourcePath, error)) {
            QFile::remove(staged);
            result.failed.append(entry.item.destinationUri);
            return result;
        }
    }

    // Only now remove the uploaded copies. Files first, then the folders this
    // upload created, deepest first, so an empty folder can be removed.
    QStringList uploadedDirs;
    for (const PreparedUndo &entry : prepared) {
        QString removeError;
        if (!Vfs::instance().remove(entry.item.destinationUri, false, &removeError)) {
            result.failed.append(entry.item.destinationUri);
            result.error += QObject::tr("Could not remove %1 from the destination: %2\n")
                                .arg(vfsLeafName(entry.item.destinationUri), removeError);
            continue;
        }
        ++result.itemsCopied;
        if (progress.itemDone)
            progress.itemDone();
    }
    for (const UploadRecord::Item &item : items) {
        if (item.isDir && item.createdHere)
            uploadedDirs.append(item.destinationUri);
    }
    std::sort(uploadedDirs.begin(), uploadedDirs.end(),
              [](const QString &a, const QString &b) { return a.size() > b.size(); });
    for (const QString &dir : uploadedDirs) {
        QString removeError;
        if (!Vfs::instance().remove(dir, false, &removeError)) {
            result.failed.append(dir);
            result.error += QObject::tr("Could not remove the uploaded folder %1: %2\n")
                                .arg(vfsLeafName(dir), removeError);
            continue;
        }
        ++result.itemsCopied;
    }

    result.error = result.error.trimmed();
    if (!result.failed.isEmpty()) {
        if (result.error.isEmpty())
            result.error = QObject::tr("The undo did not finish; the items that are listed were "
                                       "left in place.");
        if (error)
            *error = result.error;
    }
    return result;
}

FileOps::Result undoMovesOutResult(const QVector<UploadRecord::MoveOutItem> &items,
                                    QString *error, const Progress &progress)
{
    FileOps::Result result;
    result.itemsTotal = items.size();

    // Preflight: every local copy must still be the one this program downloaded,
    // and every original name must be free again. Nothing changes otherwise.
    for (const UploadRecord::MoveOutItem &item : items) {
        if (!QFileInfo::exists(item.localPath) && !QFileInfo(item.localPath).isSymLink()) {
            result.failed.append(item.localPath);
            if (error)
                *error = QObject::tr("%1 is no longer there, so it cannot be put back.")
                             .arg(item.sourceName);
            return result;
        }
        if (!item.fingerprint.isEmpty()
            && TreeSnapshot::fingerprint(item.localPath) != item.fingerprint) {
            result.failed.append(item.localPath);
            if (error) {
                *error = QObject::tr("%1 changed after it was moved here. It was left in "
                                     "place and the undo stopped.")
                             .arg(item.sourceName);
            }
            return result;
        }

        QVector<Entry> entries;
        QString listError;
        if (!Vfs::instance().list(item.sourceParentUri, true, &entries, &listError)) {
            result.failed.append(item.localPath);
            if (error)
                *error = listError;
            return result;
        }
        for (const Entry &entry : entries) {
            if (entry.name == item.sourceName) {
                result.failed.append(item.localPath);
                if (error) {
                    *error = QObject::tr("%1 already exists at the original location. It was "
                                         "not replaced and nothing was put back.")
                                 .arg(item.sourceName);
                }
                return result;
            }
        }
    }

    for (const UploadRecord::MoveOutItem &item : items) {
        if (cancelled(progress)) {
            result.cancelled = true;
            if (error)
                *error = QObject::tr("Undo cancelled.");
            return result;
        }

        // A "keep both" move gave the local copy another name. Put it back under
        // the original name first so the original state is restored exactly.
        QString localPath = item.localPath;
        if (QFileInfo(localPath).fileName() != item.sourceName) {
            const QString renamed
                = QDir(QFileInfo(localPath).absolutePath()).filePath(item.sourceName);
            if (QFileInfo::exists(renamed) || QFileInfo(renamed).isSymLink()) {
                result.failed.append(item.localPath);
                if (error) {
                    *error = QObject::tr("The original name %1 is taken in this folder; the "
                                         "copy was left where it is.")
                                 .arg(item.sourceName);
                }
                return result;
            }
            const bool renamedOk = item.isDir ? QDir().rename(localPath, renamed)
                                              : QFile::rename(localPath, renamed);
            if (!renamedOk) {
                result.failed.append(item.localPath);
                if (error)
                    *error = QObject::tr("Could not rename %1 for the undo.").arg(item.sourceName);
                return result;
            }
            localPath = renamed;
        }

        // Moving the local copy back through the verified upload path puts it
        // where it came from and removes the local one only after the read-back
        // check succeeds.
        CopyItem uploadItem;
        uploadItem.uri = QStringLiteral("file://") + localPath;
        uploadItem.isDir = item.isDir;
        Progress moveProgress = progress;
        moveProgress.conflict = [](const QString &, const QString &) {
            return FileOps::ConflictAbort;
        };
        QString uploadError;
        const Result moved
            = uploadUrisResult({uploadItem}, item.sourceParentUri, true, &uploadError,
                               moveProgress);
        if (!moved.failed.isEmpty() || moved.cancelled) {
            result.failed.append(item.localPath);
            if (error) {
                *error = uploadError.isEmpty()
                    ? QObject::tr("Could not put %1 back.").arg(item.sourceName)
                    : uploadError;
            }
            return result;
        }
        ++result.itemsCopied;
        if (progress.itemDone)
            progress.itemDone();
    }

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

bool createFile(const QString &parentDir, const QString &name, QString *newPath, QString *error)
{
    if (!PathSafety::isSafePathComponent(name)) {
        if (error)
            *error = QObject::tr("That name contains unsafe characters.");
        return false;
    }

    const QString path = PathSafety::safeJoin(parentDir, name, error);
    if (path.isEmpty())
        return false;

    QFile file(path);
    // NewOnly refuses to truncate or replace any existing file, including one
    // created between the name check and this open.
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        if (error)
            *error = QObject::tr("%1 already exists or could not be created").arg(name);
        return false;
    }
    const bool flushed = file.flush();
    file.close();
    if (!flushed) {
        QFile::remove(path);
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

    if (QFileInfo::exists(target) || QFileInfo(target).isSymLink()) {
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

bool writeFileAtomically(const QString &path, const QByteArray &data, QString *error)
{
    return AtomicFile::write(path, data, error);
}

namespace {

bool uploadParentHasEntry(const QString &parentUri, const QString &name, Entry *found = nullptr)
{
    QVector<Entry> entries;
    if (!Vfs::instance().list(parentUri, true, &entries, nullptr))
        return false;
    for (const Entry &entry : entries) {
        if (entry.name == name) {
            if (found)
                *found = entry;
            return true;
        }
    }
    return false;
}

QString uniqueUploadName(const QString &parentUri, const QString &name)
{
    QVector<Entry> entries;
    QSet<QString> taken;
    if (Vfs::instance().list(parentUri, true, &entries, nullptr)) {
        for (const Entry &entry : entries)
            taken.insert(entry.name);
    }

    const QFileInfo info(name);
    const QString base = info.completeBaseName().isEmpty() ? name : info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix();
    for (int i = 2; i < 1000; ++i) {
        const QString candidate = QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(suffix);
        if (!taken.contains(candidate))
            return candidate;
    }
    return name;
}

// Reads the uploaded entry back into a temporary file and compares it with the
// local source. An archive or server that silently truncated the file, or a
// connection that dropped after the server had accepted the bytes, is caught
// here instead of being reported as a finished move.
bool verifyUploadedFile(const QString &targetUri, const QString &localSource, QString *error,
                        QString *digestOut = nullptr)
{
    const QFileInfo local(localSource);
    QString hashError;
    const QString expected = FileHash::hexDigest(localSource, FileHash::Algorithm::Sha256, &hashError);
    if (expected.isEmpty()) {
        if (error)
            *error = hashError.isEmpty()
                ? QObject::tr("Could not read %1 for verification").arg(localSource)
                : hashError;
        return false;
    }

    QTemporaryFile temporary(QDir::tempPath()
                             + QStringLiteral("/listista-verify-XXXXXX"));
    if (!temporary.open()) {
        if (error)
            *error = QObject::tr("Could not create a temporary file for verification");
        return false;
    }
    const QString path = temporary.fileName();
    temporary.close();

    QString fetchError;
    if (!Vfs::instance().fetch(targetUri, path, &fetchError)) {
        if (error) {
            *error = QObject::tr("The upload of %1 could not be read back to verify it: %2")
                         .arg(local.fileName(), fetchError);
        }
        return false;
    }

    const QString actual
        = FileHash::hexDigest(path, FileHash::Algorithm::Sha256, &hashError);
    if (actual.isEmpty() || actual != expected) {
        if (error) {
            *error = QObject::tr("The upload of %1 does not match the source after reading it "
                                 "back. The source was kept.")
                         .arg(local.fileName());
        }
        return false;
    }
    if (digestOut)
        *digestOut = expected;
    return true;
}

enum class UploadOutcome
{
    Ok,
    Skipped,
    Failed,
    Aborted,
};

struct UploadContext
{
    const FileOps::Progress &progress;
    bool moveFiles = false;
    int *copied = nullptr;
    QStringList *notes = nullptr;
    QVector<UploadRecord::Item> *uploads = nullptr;
};

UploadOutcome uploadFile(const UploadContext &context, const QString &sourceUri,
                         const QString &localSource, const QString &targetParentUri,
                         const QString &name, QString *error)
{
    const QFileInfo local(localSource);
    if (!local.isFile() || local.isSymLink()) {
        if (error)
            *error = QObject::tr("%1 is not a regular file and cannot be uploaded")
                         .arg(local.fileName());
        return UploadOutcome::Failed;
    }

    const QString originalTarget = VfsUri::childUri(targetParentUri, name);
    QString uploadName = name;

    if (uploadParentHasEntry(targetParentUri, name)) {
        int action = FileOps::ConflictReplace;
        if (context.progress.conflict)
            action = context.progress.conflict(sourceUri, originalTarget);

        if (action == FileOps::ConflictSkip || action == FileOps::ConflictSkipAll)
            return UploadOutcome::Skipped;
        if (action == FileOps::ConflictAbort) {
            if (context.progress.requestCancel)
                context.progress.requestCancel();
            return UploadOutcome::Aborted;
        }
        if (action == FileOps::ConflictKeepBoth || action == FileOps::ConflictKeepBothAll)
            uploadName = uniqueUploadName(targetParentUri, name);
    }

    if (context.progress.current)
        context.progress.current(name);

    if (!Vfs::instance().put(targetParentUri, uploadName, localSource, error)) {
        if (error && error->isEmpty())
            *error = QObject::tr("Could not upload %1").arg(name);
        return UploadOutcome::Failed;
    }

    const QString uploadedUri = VfsUri::childUri(targetParentUri, uploadName);
    QString verifyError;
    QString digest;
    if (!verifyUploadedFile(uploadedUri, localSource, &verifyError, &digest)) {
        if (error)
            *error = verifyError;
        return UploadOutcome::Failed;
    }

    if (context.progress.step)
        context.progress.step(local.size());
    if (context.progress.itemDone)
        context.progress.itemDone();
    if (context.copied)
        ++(*context.copied);

    if (context.moveFiles) {
        if (Vfs::isLocal(sourceUri)) {
            // The item has already been counted; removing the source must not
            // report a second "item done".
            FileOps::Progress deletion = context.progress;
            deletion.itemDone = nullptr;
            if (!removeTreeLocal(Vfs::localPathOf(sourceUri), error, deletion))
                return UploadOutcome::Failed;
        } else if (context.notes) {
            context.notes->append(QObject::tr("%1 was uploaded, but the copy inside the archive "
                                              "or on the server was kept because it cannot be "
                                              "deleted safely here.")
                                      .arg(name));
        }
    }

    if (context.uploads) {
        UploadRecord::Item record;
        if (context.moveFiles && Vfs::isLocal(sourceUri))
            record.sourcePath = Vfs::localPathOf(sourceUri);
        record.destinationUri = uploadedUri;
        record.digest = digest;
        context.uploads->append(record);
    }

    return UploadOutcome::Ok;
}

UploadOutcome uploadDirectory(const UploadContext &context, const QString &sourceUri,
                              const QString &targetParentUri, const QString &name, QString *error)
{
    const QString childTarget = VfsUri::childUri(targetParentUri, name);

    bool createdHere = false;
    Entry existing;
    if (uploadParentHasEntry(targetParentUri, name, &existing)) {
        if (!existing.isDir) {
            if (error)
                *error = QObject::tr("%1 already exists as a file at the destination").arg(name);
            return UploadOutcome::Failed;
        }
    } else if (!Vfs::instance().mkdir(targetParentUri, name, error)) {
        if (error && error->isEmpty())
            *error = QObject::tr("Could not create the folder %1").arg(name);
        return UploadOutcome::Failed;
    } else {
        createdHere = true;
    }

    QVector<Entry> entries;
    if (!Vfs::instance().list(sourceUri, true, &entries, error))
        return UploadOutcome::Failed;

    bool anySkipped = false;
    for (const Entry &entry : entries) {
        if (context.progress.cancelled && context.progress.cancelled())
            return UploadOutcome::Aborted;
        if (!PathSafety::isSafePathComponent(entry.name))
            continue;

        const QString childSource = VfsUri::childUri(sourceUri, entry.name);
        if (entry.isDir) {
            const UploadOutcome nested
                = uploadDirectory(context, childSource, childTarget, entry.name, error);
            if (nested == UploadOutcome::Failed || nested == UploadOutcome::Aborted)
                return nested;
            if (nested == UploadOutcome::Skipped)
                anySkipped = true;
            continue;
        }

        const VfsUri parsed = VfsUri::parse(childSource);
        bool cleanup = false;
        QString localSource;
        if (parsed.isLocal()) {
            localSource = parsed.path;
        } else {
            QTemporaryFile temporary(QDir::tempPath()
                                     + QStringLiteral("/listista-upload-XXXXXX"));
            if (!temporary.open()) {
                if (error)
                    *error = QObject::tr("Could not create a temporary file for the upload");
                return UploadOutcome::Failed;
            }
            localSource = temporary.fileName();
            // The object dies before uploadFile() reads the file; keep the
            // file itself and remove it explicitly after the upload.
            temporary.setAutoRemove(false);
            temporary.close();
            QString fetchError;
            if (!Vfs::instance().fetch(childSource, localSource, &fetchError)) {
                if (error)
                    *error = fetchError;
                QFile::remove(localSource);
                return UploadOutcome::Failed;
            }
            cleanup = true;
        }

        const UploadOutcome uploaded
            = uploadFile(context, childSource, localSource, childTarget, entry.name, error);
        if (cleanup)
            QFile::remove(localSource);
        if (uploaded == UploadOutcome::Failed || uploaded == UploadOutcome::Aborted)
            return uploaded;
        if (uploaded == UploadOutcome::Skipped)
            anySkipped = true;
    }

    // An empty folder is only really there when the destination lists it.
    if (!anySkipped && !uploadParentHasEntry(targetParentUri, name)) {
        if (error)
            *error = QObject::tr("The folder %1 does not appear at the destination").arg(name);
        return UploadOutcome::Failed;
    }

    // Removing a local source folder only succeeds once every file inside it
    // has been uploaded and removed; skipped or failed files keep it alive.
    if (context.moveFiles && Vfs::isLocal(sourceUri)) {
        const QString sourcePath = Vfs::localPathOf(sourceUri);
        const bool empty = QDir(sourcePath)
                               .entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden
                                              | QDir::System)
                               .isEmpty();
        if (empty)
            QDir().rmdir(sourcePath);
        else if (context.notes) {
            context.notes->append(QObject::tr("The folder %1 was kept because not everything "
                                              "inside it was uploaded.")
                                      .arg(name));
        }
    }

    if (context.uploads) {
        UploadRecord::Item record;
        if (context.moveFiles && Vfs::isLocal(sourceUri))
            record.sourcePath = Vfs::localPathOf(sourceUri);
        record.destinationUri = childTarget;
        record.isDir = true;
        record.createdHere = createdHere;
        context.uploads->append(record);
    }

    return anySkipped ? UploadOutcome::Skipped : UploadOutcome::Ok;
}

} // namespace

FileOps::Result uploadUrisResult(const QVector<CopyItem> &items, const QString &targetUri,
                                 bool moveFiles, QString *error, const Progress &progress)
{
    FileOps::Result result;
    result.itemsTotal = items.size();

    if (!Vfs::instance().writable(targetUri)) {
        if (error)
            *error = QObject::tr("%1 is not writable").arg(targetUri);
        for (const CopyItem &item : items)
            result.failed.append(item.uri);
        result.error = error ? *error : QString();
        return result;
    }

    const VfsUri target = VfsUri::parse(targetUri);
    QSet<QString> names;
    QStringList problems;
    for (const CopyItem &item : items) {
        const QString name = vfsLeafName(item.uri);
        if (!PathSafety::isSafePathComponent(name)) {
            problems.append(QObject::tr("Unsafe file name: %1").arg(name));
            continue;
        }
        if (names.contains(name))
            problems.append(QObject::tr("Two selected items are both named \"%1\".").arg(name));
        names.insert(name);

        // Uploading an archive into itself would replace the container with
        // one of its own members while it is being read.
        const VfsUri source = VfsUri::parse(item.uri);
        if (target.isArchive() && source.isLocal()
            && TransferPlan::samePath(source.path, target.path)) {
            problems.append(QObject::tr("\"%1\" is the archive being written to; it cannot "
                                        "upload itself into itself.")
                                .arg(QFileInfo(target.path).fileName()));
        }
    }

    if (!problems.isEmpty()) {
        for (const CopyItem &item : items)
            result.failed.append(item.uri);
        result.error = problems.join(QLatin1Char('\n'));
        if (error)
            *error = result.error;
        return result;
    }

    Progress wrapped = progress;
    if (progress.conflict)
        wrapped.conflict = progress.conflict;

    UploadContext context{wrapped};
    context.moveFiles = moveFiles;
    context.copied = &result.itemsCopied;
    context.notes = &result.notes;
    context.uploads = &result.uploads;

    for (const CopyItem &item : items) {
        waitIfPaused(wrapped);
        if (cancelled(wrapped)) {
            result.cancelled = true;
            break;
        }

        const QString name = vfsLeafName(item.uri);
        const VfsUri parsed = VfsUri::parse(item.uri);

        // Links are not followed when uploading: the result would be a copy of
        // the target under a different name, which is not what the row says.
        if (parsed.isLocal() && QFileInfo(parsed.path).isSymLink()) {
            result.failed.append(item.uri);
            if (error)
                *error = QObject::tr("%1 is a link and cannot be uploaded").arg(name);
            continue;
        }

        UploadOutcome outcome = UploadOutcome::Failed;
        if (item.isDir) {
            outcome = uploadDirectory(context, item.uri, targetUri, name, error);
        } else {
            bool cleanup = false;
            QString localSource;
            if (parsed.isLocal()) {
                localSource = parsed.path;
            } else {
                QTemporaryFile temporary(QDir::tempPath()
                                         + QStringLiteral("/listista-upload-XXXXXX"));
                if (!temporary.open()) {
                    if (error)
                        *error = QObject::tr("Could not create a temporary file for the upload");
                    result.failed.append(item.uri);
                    continue;
                }
                localSource = temporary.fileName();
                // The object goes out of scope before uploadFile() reads the
                // file; keep it and remove it explicitly after the upload.
                temporary.setAutoRemove(false);
                temporary.close();
                QString fetchError;
                if (!Vfs::instance().fetch(item.uri, localSource, &fetchError)) {
                    if (error)
                        *error = fetchError;
                    QFile::remove(localSource);
                    result.failed.append(item.uri);
                    continue;
                }
                cleanup = true;
            }

            outcome = uploadFile(context, item.uri, localSource, targetUri, name, error);
            if (cleanup)
                QFile::remove(localSource);
        }

        switch (outcome) {
        case UploadOutcome::Ok:
            break;
        case UploadOutcome::Skipped:
            ++result.itemsSkipped;
            ++result.skipped;
            break;
        case UploadOutcome::Aborted:
            result.cancelled = true;
            break;
        case UploadOutcome::Failed:
            result.failed.append(item.uri);
            break;
        }
    }

    result.cancelled = result.cancelled || (wrapped.cancelled && wrapped.cancelled());
    result.error = error ? *error : QString();
    return result;
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

QString trashFolderPath()
{
    return trashBaseDir() + QStringLiteral("/files");
}

TrashResult moveToTrashDetailed(const QStringList &paths)
{
    TrashResult result;
    const QString base = trashBaseDir();
    const QString filesDir = base + QStringLiteral("/files");
    const QString infoDir = base + QStringLiteral("/info");

    if (!QDir().mkpath(filesDir) || !QDir().mkpath(infoDir)) {
        result.error = QObject::tr("Could not create the trash folder in %1").arg(base);
        result.failed = paths;
        return result;
    }

    const Progress noProgress;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (!info.exists() && !info.isSymLink()) {
            result.failed.append(path);
            result.errors.append(QObject::tr("%1 does not exist").arg(QFileInfo(path).fileName()));
            continue;
        }

        const QString trashName = uniqueTrashName(filesDir, info.fileName());
        const QString targetPath = QDir(filesDir).filePath(trashName);
        const QString infoPath = QDir(infoDir).filePath(trashName + QStringLiteral(".trashinfo"));

        QFile infoFile(infoPath);
        if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            result.failed.append(path);
            result.errors.append(QObject::tr("Could not write %1").arg(infoPath));
            continue;
        }

        const QByteArray encodedPath = QUrl::toPercentEncoding(info.absoluteFilePath(), "/");
        infoFile.write("[Trash Info]\nPath=" + encodedPath + "\nDeletionDate="
                       + QDateTime::currentDateTime()
                             .toString(QStringLiteral("yyyy-MM-ddThh:mm:ss"))
                             .toUtf8()
                       + "\n");
        infoFile.close();

        if (QFile::rename(info.absoluteFilePath(), targetPath)) {
            result.trashed.append(path);
            result.names.append(trashName);
            continue;
        }

        // Different filesystem: copy across, then remove the original. The
        // original is removed only after the copy has been independently
        // verified; a failed or cancelled copy must never take the source.
        QString copyError;
        const FileOps::CopyStatus copied = copyRecursivelyImpl(info.absoluteFilePath(), targetPath,
                                                               &copyError, noProgress, true);
        if (copied != FileOps::CopyOk) {
            QFile::remove(infoPath);
            removeTreeLocal(targetPath, nullptr, noProgress);
            result.failed.append(path);
            result.errors.append(copyError.isEmpty()
                                     ? QObject::tr("Could not move %1 to the trash").arg(info.fileName())
                                     : copyError);
            continue;
        }

        const TreeSnapshot::Tree source = TreeSnapshot::capture(info.absoluteFilePath());
        const TreeSnapshot::Tree trash = TreeSnapshot::capture(targetPath);
        const QStringList differences = TreeSnapshot::missing(source, trash);
        if (!differences.isEmpty()) {
            QFile::remove(infoPath);
            removeTreeLocal(targetPath, nullptr, noProgress);
            result.failed.append(path);
            result.errors.append(QObject::tr("Verification failed for %1: %2")
                                     .arg(info.fileName(),
                                          differences.mid(0, 4).join(QStringLiteral("; "))));
            continue;
        }

        if (!removeTreeLocal(info.absoluteFilePath(), &copyError, noProgress)) {
            // Keep both copies: never claim an item was trashed when its
            // original is still there.
            QFile::remove(infoPath);
            result.failed.append(path);
            result.errors.append(copyError.isEmpty()
                                     ? QObject::tr("Could not remove %1 after copying it to the trash")
                                           .arg(info.fileName())
                                     : copyError);
            continue;
        }

        result.trashed.append(path);
        result.names.append(trashName);
    }

    result.error = result.errors.join(QLatin1Char('\n'));
    return result;
}

// Own implementation of the freedesktop trash specification instead of
// QFile::moveToTrash, because the Qt variant refuses to create a missing trash
// directory and leaves the user with no way to delete anything.
bool moveToTrash(const QStringList &paths, QStringList *trashNames, QString *error)
{
    const TrashResult result = moveToTrashDetailed(paths);
    if (trashNames)
        *trashNames = result.names;
    if (error)
        *error = result.error;
    return result.failed.isEmpty();
}

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
        if (!removeTreeLocal(path, error, progress))
            return false;
    }
    return true;
}

namespace {

// Reads the original location recorded in a freedesktop .trashinfo file.
QString trashOriginalPath(const QString &infoPath)
{
    QFile infoFile(infoPath);
    if (!infoFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    const QString content = QString::fromUtf8(infoFile.readAll());
    infoFile.close();
    for (const QString &line : content.split(QLatin1Char('\n'))) {
        if (line.startsWith(QStringLiteral("Path=")))
            return QUrl::fromPercentEncoding(line.mid(5).toUtf8());
    }
    return QString();
}

} // namespace

bool restoreFromTrash(const QStringList &trashNames, QString *error)
{
    const QString base = trashBaseDir();
    const QString filesDir = base + QStringLiteral("/files");
    const QString infoDir = base + QStringLiteral("/info");

    for (const QString &name : trashNames) {
        const QString infoPath = QDir(infoDir).filePath(name + QStringLiteral(".trashinfo"));
        const QString originalPath = trashOriginalPath(infoPath);
        if (originalPath.isEmpty()) {
            if (error)
                *error = QObject::tr("Missing original path for %1").arg(name);
            return false;
        }

        const QString src = QDir(filesDir).filePath(name);
        // If something already occupies the original path, pick a unique name.
        QString dest = originalPath;
        if (QFileInfo::exists(dest) || QFileInfo(dest).isSymLink()) {
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
            const TreeSnapshot::Tree before = TreeSnapshot::capture(src);
            QString copyError;
            const FileOps::CopyStatus copied
                = copyRecursivelyImpl(src, dest, &copyError, Progress(), true);
            if (copied != FileOps::CopyOk) {
                removeTreeLocal(dest, nullptr, Progress());
                if (error)
                    *error = copyError.isEmpty() ? QObject::tr("Could not restore %1").arg(name)
                                                : copyError;
                return false;
            }
            const QStringList differences
                = TreeSnapshot::missing(before, TreeSnapshot::capture(dest));
            if (!differences.isEmpty()) {
                if (error)
                    *error = QObject::tr("Verification failed for %1: %2")
                                 .arg(name, differences.mid(0, 4).join(QStringLiteral("; ")));
                return false;
            }
            if (!removeTreeLocal(src, &copyError, Progress())) {
                if (error)
                    *error = copyError.isEmpty()
                                 ? QObject::tr("Could not remove %1 from the trash after restoring it")
                                       .arg(name)
                                 : copyError;
                return false;
            }
        }

        QFile::remove(infoPath);
    }

    return true;
}

bool restoreLocalPaths(const QStringList &paths, QStringList *restored, QString *error)
{
    const QString base = trashBaseDir();
    const QString filesDir = base + QStringLiteral("/files");
    const QString infoDir = base + QStringLiteral("/info");

    QHash<QString, QString> byOriginalPath;
    QDir info(infoDir);
    for (const QString &fileName : info.entryList({QStringLiteral("*.trashinfo")}, QDir::Files)) {
        const QString original = trashOriginalPath(QDir(infoDir).filePath(fileName));
        if (original.isEmpty())
            continue;
        const QString name = fileName.left(fileName.size() - QStringLiteral(".trashinfo").size());
        byOriginalPath.insert(TransferPlan::normalised(original), name);
    }

    QStringList names;
    QStringList failures;
    for (const QString &path : paths) {
        const QString normalised = TransferPlan::normalised(path);
        const auto found = byOriginalPath.constFind(normalised);
        if (found != byOriginalPath.constEnd()) {
            names.append(found.value());
            continue;
        }

        // A selected entry inside the trash itself is restored by its name.
        if (TransferPlan::isInside(normalised, filesDir)
            && QFileInfo(QDir(filesDir).filePath(QFileInfo(path).fileName())).exists()) {
            names.append(QFileInfo(path).fileName());
            continue;
        }

        failures.append(QObject::tr("%1 is not in the trash").arg(QFileInfo(path).fileName()));
    }

    QString restoreError;
    if (!names.isEmpty() && !restoreFromTrash(names, &restoreError))
        failures.append(restoreError);

    if (restored)
        *restored = names;
    if (error)
        *error = failures.join(QLatin1Char('\n'));
    return failures.isEmpty();
}

bool emptyTrash(QString *error)
{
    const QString base = trashBaseDir();
    const QString filesDir = base + QStringLiteral("/files");
    const QString infoDir = base + QStringLiteral("/info");

    const Progress noProgress;
    QDir files(filesDir);
    for (const QString &name : files.entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        // A folder symlink in the trash is a link, not the folder it points at.
        if (!removeTreeLocal(filesDir + QLatin1Char('/') + name, error, noProgress))
            return false;
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
    return copyRecursivelyImpl(source, target, error, progress, false);
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
