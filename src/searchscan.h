#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <functional>

// A recursive search over a local folder tree. Kept free of widgets so the same
// scan can run in a worker thread, in a test or from a script. Name, extension,
// size and date filters match Listi's search; content search is an addition of
// the Qt port.
namespace SearchScan {

struct Query
{
    QString nameContains;      // case-insensitive substring of the file name
    QString extension;         // without the dot; empty means any
    qint64 minBytes = 0;       // 0 = no lower bound
    qint64 maxBytes = -1;      // < 0 = no upper bound
    QDateTime modifiedAfter;   // invalid = no date filter
    bool includeFolders = false;
    bool includeHidden = false;
    // When true and nameContains is set, a regular text file also matches when
    // its contents contain the text. Binary files are never read for content.
    bool searchContents = false;
    qint64 maxContentBytes = 2 * 1024 * 1024;
    int maximumResults = 5000;
};

struct Result
{
    QStringList matches;        // absolute local paths, in scan order
    QStringList unreadableFolders;
    int scannedEntries = 0;
    int contentFilesRead = 0;
    bool cancelled = false;
    bool truncated = false;
};

struct Callbacks
{
    // Return true to stop. Polled for every entry, so a huge folder answers a
    // cancel promptly.
    std::function<bool()> cancelled;
    // Called with the entry the scan is looking at, for a progress line.
    std::function<void(const QString &)> current;
    // Called with the new match count whenever an entry is added.
    std::function<void(int)> matched;
};

// Runs the scan synchronously. The caller is responsible for putting it on a
// worker thread when the tree can be large.
Result run(const QString &root, const Query &query, const Callbacks &callbacks = Callbacks());

// True when `path` is a readable text file no larger than `maxBytes` whose
// contents contain `needle` (case-insensitive). Sets `error` when the file
// could not be read. Binary files (a NUL byte in the first 8 KB) return false
// with an empty error.
bool contentMatches(const QString &path, const QString &needle, qint64 maxBytes,
                    QString *error = nullptr);

// The sentence shown above a result list, so limit, cancellation, completion
// and unreadable folders are never confused with one another.
QString summary(const Result &result);

} // namespace SearchScan
