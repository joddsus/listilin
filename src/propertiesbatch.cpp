#include "propertiesbatch.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace {

// Qt exposes each owner permission twice (ReadOwner/ReadUser and friends) but
// both map to the same Unix bit. Reading a file reports both aliases, while
// setPermissions() must be given both for a bit to come out right; clearing
// only QFile::WriteOwner would leave QFile::WriteUser behind and the file
// writable. Expand every change and every read to the full pair.
QFile::Permissions withOwnerAliases(QFile::Permissions permissions)
{
    struct Pair
    {
        QFile::Permission owner;
        QFile::Permission user;
    };
    static const Pair pairs[] = {
        {QFile::ReadOwner, QFile::ReadUser},
        {QFile::WriteOwner, QFile::WriteUser},
        {QFile::ExeOwner, QFile::ExeUser},
    };
    for (const Pair &pair : pairs) {
        if (permissions & pair.owner)
            permissions |= pair.user;
        if (permissions & pair.user)
            permissions |= pair.owner;
    }
    return permissions;
}

bool applyOne(const QString &path, const PropertiesBatch::Change &change, QString *why)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        *why = QStringLiteral("does not exist");
        return false;
    }
    if (info.isSymLink())
        return true;

    QFile file(path);
    const QFile::Permissions current = withOwnerAliases(file.permissions());
    const QFile::Permissions next = (current & ~withOwnerAliases(change.clear))
        | withOwnerAliases(change.set);
    if (next == current)
        return true;
    if (!file.setPermissions(next)) {
        *why = QStringLiteral("permissions could not be changed");
        return false;
    }
    return true;
}

} // namespace

namespace PropertiesBatch {

Change changesFromStates(const QVector<BoxState> &before, const QVector<BoxState> &after,
                         const QVector<QFile::Permissions> &bits)
{
    Change change;
    if (before.size() != after.size() || before.size() != bits.size())
        return change;

    for (int i = 0; i < bits.size(); ++i) {
        if (before.at(i) == after.at(i))
            continue;
        if (after.at(i) == BoxState::Mixed)
            continue;
        if (after.at(i) == BoxState::On)
            change.set |= bits.at(i);
        else
            change.clear |= bits.at(i);
    }
    return change;
}

Summary summarize(const QStringList &localPaths)
{
    Summary summary;
    QString location;
    bool first = true;

    for (const QString &path : localPaths) {
        const QFileInfo info(path);
        if (info.isDir())
            ++summary.folders;
        else
            ++summary.files;
        if (info.isFile())
            summary.totalSize += info.size();

        const QString parent = info.absolutePath();
        if (first) {
            location = parent;
            first = false;
        } else if (location != parent) {
            location.clear();
        }
    }

    summary.location = location;
    return summary;
}

QStringList apply(const QStringList &localPaths, const Change &change, bool recursive)
{
    QStringList failures;
    if (change.isEmpty())
        return failures;

    QStringList targets;
    for (const QString &path : localPaths) {
        targets.append(path);
        if (!recursive || !QFileInfo(path).isDir())
            continue;

        // Collect first, then change from the deepest entries upwards. Taking
        // away read or execute permission on a folder before reading its
        // children would make the rest of the tree unreachable.
        QStringList children;
        QDirIterator it(path, QDir::AllEntries | QDir::Hidden | QDir::System
                                  | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext())
            children.append(it.next());
        std::sort(children.begin(), children.end(), [](const QString &a, const QString &b) {
            return a.count(QLatin1Char('/')) > b.count(QLatin1Char('/'));
        });
        targets.append(children);
    }

    QSet<QString> seen;
    for (const QString &path : targets) {
        if (seen.contains(path))
            continue;
        seen.insert(path);
        QString why;
        if (!applyOne(path, change, &why))
            failures.append(QStringLiteral("%1: %2").arg(path, why));
    }
    return failures;
}

} // namespace PropertiesBatch
