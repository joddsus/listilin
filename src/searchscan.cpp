#include "searchscan.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include "pathsafety.h"
#include "vfs.h"

namespace {

bool isBinary(const QByteArray &head)
{
    return head.contains('\0');
}

bool readHeadIsBinary(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return true;
    const QByteArray head = file.read(8192);
    return isBinary(head);
}

bool matchesFilters(const SearchScan::Query &query, const QString &name, qint64 size,
                    const QDateTime &modified)
{
    if (!query.extension.isEmpty()) {
        QString wanted = query.extension.toLower();
        if (wanted.startsWith(QLatin1Char('.')))
            wanted.remove(0, 1);
        if (QFileInfo(name).suffix().toLower() != wanted)
            return false;
    }
    if (query.minBytes > 0 && size < query.minBytes)
        return false;
    if (query.maxBytes >= 0 && size > query.maxBytes)
        return false;
    if (query.modifiedAfter.isValid() && modified < query.modifiedAfter)
        return false;
    return true;
}

bool matchesName(const SearchScan::Query &query, const QString &name)
{
    return query.nameContains.isEmpty()
        || name.contains(query.nameContains, Qt::CaseInsensitive);
}

} // namespace

namespace SearchScan {

bool contentMatches(const QString &path, const QString &needle, qint64 maxBytes, QString *error)
{
    if (needle.isEmpty())
        return false;

    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink())
        return false;
    if (maxBytes > 0 && info.size() > maxBytes)
        return false;
    if (readHeadIsBinary(path))
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QObject::tr("Could not read %1").arg(path);
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    return QString::fromUtf8(data).contains(needle, Qt::CaseInsensitive);
}

Result run(const QString &root, const Query &query, const Callbacks &callbacks)
{
    Result result;
    const int maximumResults = query.maximumResults > 0 ? query.maximumResults : 0;

    const auto stopped = [&]() {
        if (result.cancelled || result.truncated)
            return true;
        if (callbacks.cancelled && callbacks.cancelled()) {
            result.cancelled = true;
            return true;
        }
        return false;
    };

    // The name is checked separately from the other filters: a content match
    // is allowed to name a file whose name does not contain the text.
    const auto filtersPass = [&](const QFileInfo &info) {
        return matchesFilters(query, info.fileName(), info.size(), info.lastModified());
    };

    std::function<void(const QString &)> scan = [&](const QString &dirPath) {
        if (stopped())
            return;

        QDir dir(dirPath);
        if (!dir.isReadable()) {
            result.unreadableFolders.append(dirPath);
            return;
        }

        QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
        if (query.includeHidden)
            filters |= QDir::Hidden;

        const QFileInfoList children = dir.entryInfoList(filters, QDir::Name);
        for (const QFileInfo &info : children) {
            if (stopped())
                return;

            ++result.scannedEntries;
            if (callbacks.current)
                callbacks.current(info.absoluteFilePath());

            // A symlinked folder is never followed: it could loop, and its
            // contents belong to another place in the tree.
            if (info.isDir() && !info.isSymLink()) {
                if (query.includeFolders && filtersPass(info)
                    && matchesName(query, info.fileName())) {
                    result.matches.append(info.absoluteFilePath());
                    if (callbacks.matched)
                        callbacks.matched(int(result.matches.size()));
                    if (maximumResults > 0 && result.matches.size() >= maximumResults) {
                        result.truncated = true;
                        return;
                    }
                }
                scan(info.absoluteFilePath());
                continue;
            }

            if (info.isDir())
                continue;

            if (!filtersPass(info))
                continue;

            bool matched = matchesName(query, info.fileName());

            // Content search is an extra way to match, never a replacement for
            // a name match: a file whose name matches is listed even when its
            // content is binary.
            if (!matched && query.searchContents && !query.nameContains.isEmpty()) {
                ++result.contentFilesRead;
                matched = contentMatches(info.absoluteFilePath(), query.nameContains,
                                         query.maxContentBytes, nullptr);
            }

            if (!matched)
                continue;

            result.matches.append(info.absoluteFilePath());
            if (callbacks.matched)
                callbacks.matched(int(result.matches.size()));
            if (maximumResults > 0 && result.matches.size() >= maximumResults) {
                result.truncated = true;
                return;
            }
        }
    };

    scan(root);
    return result;
}

Result runVfs(const QString &rootUri, const Query &query, const Callbacks &callbacks)
{
    Result result;
    // Contents live on the other side of the connection and downloading every
    // file to grep it is not a search; say so instead of silently ignoring it.
    result.contentSearchSkipped = query.searchContents;

    const int maximumResults = query.maximumResults > 0 ? query.maximumResults : 0;
    const auto stopped = [&]() {
        if (result.cancelled || result.truncated)
            return true;
        if (callbacks.cancelled && callbacks.cancelled()) {
            result.cancelled = true;
            return true;
        }
        return false;
    };

    std::function<void(const QString &)> scan = [&](const QString &dirUri) {
        if (stopped())
            return;

        QVector<Entry> entries;
        QString listError;
        if (!Vfs::instance().list(dirUri, query.includeHidden, &entries, &listError)) {
            // One unreadable folder (permissions, a dropped connection) must
            // not hide the matches found elsewhere.
            result.unreadableFolders.append(dirUri);
            return;
        }

        for (const Entry &entry : entries) {
            if (stopped())
                return;
            if (!PathSafety::isSafePathComponent(entry.name))
                continue;

            ++result.scannedEntries;
            const QString child = VfsUri::childUri(dirUri, entry.name);
            if (callbacks.current)
                callbacks.current(child);

            if (entry.isDir) {
                if (query.includeFolders
                    && matchesFilters(query, entry.name, entry.size, entry.modified)
                    && matchesName(query, entry.name)) {
                    result.matches.append(child);
                    if (callbacks.matched)
                        callbacks.matched(int(result.matches.size()));
                    if (maximumResults > 0 && result.matches.size() >= maximumResults) {
                        result.truncated = true;
                        return;
                    }
                }
                scan(child);
                continue;
            }

            if (!matchesFilters(query, entry.name, entry.size, entry.modified)
                || !matchesName(query, entry.name)) {
                continue;
            }

            result.matches.append(child);
            if (callbacks.matched)
                callbacks.matched(int(result.matches.size()));
            if (maximumResults > 0 && result.matches.size() >= maximumResults) {
                result.truncated = true;
                return;
            }
        }
    };

    scan(rootUri);
    return result;
}

QString summary(const Result &result)
{
    QString text;
    if (result.truncated) {
        text = QObject::tr("Showing the first %1 matches; there are more. Narrow the "
                           "search to see the rest.")
                   .arg(result.matches.size());
    } else if (result.cancelled) {
        text = QObject::tr("Search cancelled after %1 matches.").arg(result.matches.size());
    } else {
        text = QObject::tr("%1 matches").arg(result.matches.size());
    }

    if (result.contentFilesRead > 0)
        text += QLatin1Char('\n')
            + QObject::tr("%1 file(s) were read to look inside their contents.")
                  .arg(result.contentFilesRead);

    if (result.contentSearchSkipped) {
        text += QLatin1Char('\n')
            + QObject::tr("Content search reads local files only; this search matched "
                          "names, sizes and dates.");
    }

    if (!result.unreadableFolders.isEmpty()) {
        text += QLatin1Char('\n')
            + QObject::tr("%1 folder(s) could not be read, so the list may be incomplete: %2")
                  .arg(result.unreadableFolders.size())
                  .arg(result.unreadableFolders.mid(0, 3).join(QStringLiteral(", ")));
    }
    return text;
}

} // namespace SearchScan
