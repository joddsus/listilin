#include "recentfolders.h"

namespace RecentFolders {

QStringList mostRecent(const QStringList &history, int maxEntries,
                       const QString &excludeCurrent)
{
    QStringList recent;
    for (int i = history.size() - 1; i >= 0 && recent.size() < maxEntries; --i) {
        const QString &uri = history.at(i);
        if (uri.isEmpty() || uri == excludeCurrent || recent.contains(uri))
            continue;
        recent.append(uri);
    }
    return recent;
}

} // namespace RecentFolders
