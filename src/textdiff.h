#pragma once

#include <QString>
#include <QStringList>

// Line-oriented text diff (unified format). Kept free of widgets so the
// algorithm can be tested without a GUI.
namespace TextDiff {

// Returns a unified diff of `oldLines` vs `newLines` with `context` lines of
// surrounding context around each change.
QString unifiedDiff(const QStringList &oldLines, const QStringList &newLines,
                    int context = 3);

// Convenience: splits text into lines and diffs it.
QString unifiedDiff(const QString &oldText, const QString &newText, int context = 3);

} // namespace TextDiff