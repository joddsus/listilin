#include "duplicatefinder.h"
#include "filehash.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QHash>
#include <algorithm>
#include <QFileInfo>

namespace DuplicateFinder {

QList<Group> findDuplicates(const QString &root, int minSizeBytes)
{
    QList<Group> result;
    if (minSizeBytes < 1)
        minSizeBytes = 1;

    // First pass: group by size.
    QHash<qint64, QStringList> bySize;
    QDirIterator it(root, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        if (info.size() < minSizeBytes)
            continue;
        bySize[info.size()].append(info.absoluteFilePath());
    }

    // Only sizes with more than one candidate can be duplicates.
    for (auto sizeIt = bySize.constBegin(); sizeIt != bySize.constEnd(); ++sizeIt) {
        if (sizeIt.value().size() < 2)
            continue;

        // Second pass: hash candidates and group by digest.
        QHash<QString, QStringList> byDigest;
        for (const QString &path : sizeIt.value()) {
            QString error;
            const QString digest = FileHash::hexDigest(path, FileHash::Algorithm::Sha256, &error);
            if (digest.isEmpty())
                continue;
            byDigest[digest].append(path);
        }

        for (auto digestIt = byDigest.constBegin(); digestIt != byDigest.constEnd(); ++digestIt) {
            if (digestIt.value().size() < 2)
                continue;
            Group group;
            group.size = sizeIt.key();
            group.digest = digestIt.key();
            group.paths = digestIt.value();
            group.paths.sort();
            result.append(group);
        }
    }

    // Sort groups by size descending, then digest for determinism.
    std::sort(result.begin(), result.end(), [](const Group &a, const Group &b) {
        if (a.size != b.size)
            return a.size > b.size;
        return a.digest < b.digest;
    });

    return result;
}

} // namespace DuplicateFinder