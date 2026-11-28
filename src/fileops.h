#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

#include "uploadrecord.h"

// File operations used by the commands. Kept free of UI so that the same code
// can run in a worker thread or be driven by a script.
namespace FileOps {

// Outcome of a recursive copy. Distinct from a plain bool so callers can tell
// a copy that completed from one that was skipped, aborted or failed, and avoid
// deleting a source that was not fully copied.
enum CopyStatus
{
    CopyOk = 0,
    CopySkipped,
    CopyAborted,
    CopyFailed,
    // Aliases for callers/tests that use a different vocabulary.
    Copied = CopyOk,
    Success = CopyOk,
    Ok = CopyOk,
    Skipped = CopySkipped,
    Aborted = CopyAborted,
    Cancelled = CopyAborted,
    Failed = CopyFailed,
};
using CopyResult = CopyStatus;
using CopyOutcome = CopyStatus;

// What to do when a destination already exists. Returned by the conflict
// callback; the "All" variants make the choice sticky for the rest of the job.
enum ConflictAction
{
    ConflictReplace = 0,
    ConflictSkip,
    ConflictReplaceAll,
    ConflictSkipAll,
    ConflictKeepBoth,
    ConflictKeepBothAll,
    ConflictKeep = ConflictKeepBoth,
    ConflictAbort,
};

// How an operation tells the outside world what it is doing. Every callback is
// optional: pass none and the operation simply runs to completion, which is
// what the tests and any later scripted API want.
struct Progress
{
    std::function<void(qint64 totalBytes)> total;
    std::function<void(qint64 bytes)> step;
    std::function<void()> itemDone;
    std::function<void(const QString &current)> current;
    std::function<bool()> cancelled;
    // Returns true while the job should wait. Polled together with cancelled.
    std::function<bool()> paused;
    // Flip the cancelled flag so the whole job stops. Used by conflict Abort.
    std::function<void()> requestCancel;
    // Asked before overwriting an existing destination. Returns a ConflictAction.
    // When null, collisions are auto-renamed (the old behaviour).
    std::function<int(const QString &source, const QString &target)> conflict;
};

inline bool cancelled(const Progress &progress)
{
    return progress.cancelled && progress.cancelled();
}

// Structured outcome of a copy/move operation: failed paths, error text and
// byte/item counts for progress reporting and job results.
struct Result
{
    QStringList failed;
    QString error;
    qint64 bytesCopied = 0;
    qint64 bytesTotal = 0;
    int itemsCopied = 0;
    int itemsSkipped = 0;
    int skipped = 0;
    int itemsTotal = 0;
    bool cancelled = false;
    // Things the caller should tell the user even on success, for example a
    // source that was deliberately kept because it lives inside an archive.
    QStringList notes;
    // Exactly what an upload wrote, so it can be undone with a digest check.
    QVector<UploadRecord::Item> uploads;
    // Local copies produced by moving entries out of an archive/remote folder.
    QVector<UploadRecord::MoveOutItem> movesOut;
};

// Copies or moves every path into targetDir. Returns the paths that failed.
QStringList copyAll(const QStringList &sources, const QString &targetDir, bool moveFiles,
                    QString *error, const Progress &progress = Progress());

// Structured version of copyAll.
Result copyAllResult(const QStringList &sources, const QString &targetDir, bool moveFiles,
                     QString *error, const Progress &progress = Progress());

// Recursive copy of one file or folder (public so other modules can reuse it).
CopyStatus copyRecursively(const QString &source, const QString &target, QString *error,
                          const Progress &progress = Progress());

// One thing to transfer. isDir matters because a folder inside an archive or
// on a remote site has to be walked, not fetched as a single object.
struct CopyItem
{
    QString uri;
    bool isDir = false;
};

// Same as copyAll, but for URIs: local files are copied directly, archive and
// remote entries are fetched out of their backend first.
QStringList copyUris(const QVector<CopyItem> &items, const QString &targetDir, bool moveFiles,
                     QString *error, const Progress &progress = Progress());

// Structured version of copyUris.
Result copyUrisResult(const QVector<CopyItem> &items, const QString &targetDir, bool moveFiles,
                      QString *error, const Progress &progress = Progress());

// Uploads files and folders into an archive or remote folder through the VFS.
// Every uploaded file is read back and compared with the local source (size and
// SHA-256) before it counts as done. With moveFiles a local source is removed
// only after that verification; a source that lives inside an archive or on a
// server is kept, and the reason is reported in `notes`.
Result uploadUrisResult(const QVector<CopyItem> &items, const QString &targetUri,
                        bool moveFiles, QString *error,
                        const Progress &progress = Progress());

// Reverses uploads: an uploaded file whose local source was moved is read back,
// checked against the digest recorded at upload time, written back to its
// original path atomically, and only then removed from the archive or server. A
// copy whose source was kept is only removed after the same digest check.
// Nothing is removed when a destination changed or an original name is taken
// by something else.
Result undoUploadsResult(const QVector<UploadRecord::Item> &items, QString *error,
                         const Progress &progress = Progress());

// Reverses a move out of an archive or remote folder: the local copy is checked
// against the fingerprint recorded when it arrived, then uploaded back to where
// it came from (verified read-back) and removed locally. It refuses when the
// local copy changed or the original name is occupied again.
Result undoMovesOutResult(const QVector<UploadRecord::MoveOutItem> &items, QString *error,
                          const Progress &progress = Progress());

bool makeDirectory(const QString &parentDir, const QString &name, QString *newPath,
                   QString *error);
// Creates a new empty file without replacing an existing entry. The name must
// be a single safe path component; the returned path is contained in parentDir.
bool createFile(const QString &parentDir, const QString &name, QString *newPath,
                QString *error);
bool rename(const QString &path, const QString &newName, QString *newPath, QString *error);
// Result of sending several items to the trash. Items are handled one by one:
// a failure on the second file must not undo or misreport the first.
struct TrashResult
{
    QStringList trashed; // original paths that are now in the trash
    QStringList names;   // their names inside the trash
    QStringList failed;  // original paths that stayed where they were
    QStringList errors;  // one message per failed item
    QString error;       // all messages, joined
};

// The folder the freedesktop trash stores deleted items in, so the window can
// tell a trash view from an ordinary folder.
QString trashFolderPath();
TrashResult moveToTrashDetailed(const QStringList &paths);
bool moveToTrash(const QStringList &paths, QStringList *trashNames = nullptr, QString *error = nullptr);
bool removePermanently(const QStringList &paths, QString *error,
                       const Progress &progress = Progress());

// Restores items that live in the freedesktop trash back to their original
// location. `trashNames` are file names inside ~/.local/share/Trash/files.
bool restoreFromTrash(const QStringList &trashNames, QString *error);
// Restores the trash entries whose recorded original path is one of `paths`,
// so a normal folder never restores an unrelated file that happens to share a
// name with something in the trash. Also accepts entries selected in the trash.
bool restoreLocalPaths(const QStringList &paths, QStringList *restored, QString *error);

// Replaces a file with `data` through a staged sibling, so a failed save can
// never truncate the original. Used by the text editor.
bool writeFileAtomically(const QString &path, const QByteArray &data, QString *error);
// Empties the trash completely.
bool emptyTrash(QString *error);

} // namespace FileOps
