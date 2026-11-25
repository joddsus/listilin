#pragma once

#include <QString>
#include <QStringList>

// Builds the per-panel Recent folders list from navigation history. Kept pure
// so duplicate and ordering rules are testable without a window.
namespace RecentFolders {

QStringList mostRecent(const QStringList &history, int maxEntries = 12,
                       const QString &excludeCurrent = QString());

} // namespace RecentFolders
