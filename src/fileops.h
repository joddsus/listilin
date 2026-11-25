#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

// File operations used by the commands. Kept free of UI so that the same code
// can run in a worker thread or be driven by a script.
namespace FileOps {

// What to do when a destination already exists. Returned by the conflict
// callback; the "All" variants make the choice sticky for the rest of the job.
enum ConflictAction
{
    ConflictReplace = 0,
    ConflictSkip,
    ConflictReplaceAll,
    ConflictSkipAll,
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
    int itemsTotal = 0;
    bool cancelled = false;
};

// Copies or moves every path into targetDir. Returns the paths that failed.
QStringList copyAll(const QStringList &sources, const QString &targetDir, bool moveFiles,
                    QString *error, const Progress &progress = Progress());

// Structured version of copyAll.
Result copyAllResult(const QStringList &sources, const QString &targetDir, bool moveFiles,
                     QString *error, const Progress &progress = Progress());

// Recursive copy of one file or folder (public so other modules can reuse it).
bool copyRecursively(const QString &source, const QString &target, QString *error,
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

bool makeDirectory(const QString &parentDir, const QString &name, QString *newPath,
                   QString *error);
bool rename(const QString &path, const QString &newName, QString *newPath, QString *error);
bool moveToTrash(const QStringList &paths, QStringList *trashNames = nullptr, QString *error = nullptr);
bool removePermanently(const QStringList &paths, QString *error,
                       const Progress &progress = Progress());

// Restores items that live in the freedesktop trash back to their original
// location. `trashNames` are file names inside ~/.local/share/Trash/files.
bool restoreFromTrash(const QStringList &trashNames, QString *error);
// Empties the trash completely.
bool emptyTrash(QString *error);

} // namespace FileOps
