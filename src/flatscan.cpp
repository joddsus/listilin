#include "flatscan.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QObject>

namespace FlatScan {

QVector<Entry> scanLocal(const QString &root, bool showHidden, QString *error, int maxEntries)
{
    QVector<Entry> entries;
    if (!QFileInfo(root).isDir()) {
        if (error)
            *error = QObject::tr("Flat View works in a local folder.");
        return entries;
    }

    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (showHidden)
        filters |= QDir::Hidden;

    QDir base(root);
    QDirIterator it(root, filters, QDirIterator::Subdirectories);
    while (it.hasNext() && entries.size() < maxEntries) {
        it.next();
        const QFileInfo info = it.fileInfo();
        const QString relative = base.relativeFilePath(info.absoluteFilePath());
        if (relative.isEmpty() || relative == QLatin1String("."))
            continue;

        Entry entry;
        entry.name = QDir::fromNativeSeparators(relative);
        entry.size = info.isDir() ? 0 : info.size();
        entry.hasSize = !info.isDir();
        entry.modified = info.lastModified();
        entry.isDir = info.isDir();
        entries.append(entry);
    }

    return entries;
}

} // namespace FlatScan
