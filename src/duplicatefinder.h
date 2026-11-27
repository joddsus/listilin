#pragma once

#include <QString>
#include <QStringList>

#include <QList>

#include <functional>

// Finds duplicate files by size and content hash. Kept free of widgets so the
// grouping rules can be tested without a GUI.
namespace DuplicateFinder {

struct Group
{
    qint64 size = 0;
    QString digest;
    QStringList paths;
};

// Scans `root` recursively and returns groups of files that share the same
// size and SHA-256 digest. Groups with fewer than two files are omitted.
// `cancelled` is polled while walking and hashing; when it returns true the
// groups found so far are returned. A negative result is not an error, so the
// caller cannot mistake a cancelled scan for a clean folder.
QList<Group> findDuplicates(const QString &root, int minSizeBytes = 1,
                            const std::function<bool()> &cancelled = {});

} // namespace DuplicateFinder