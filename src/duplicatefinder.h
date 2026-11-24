#pragma once

#include <QString>
#include <QStringList>

#include <QList>

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
QList<Group> findDuplicates(const QString &root, int minSizeBytes = 1);

} // namespace DuplicateFinder