#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// Line-oriented text diff (unified format). Kept free of widgets so the
// algorithm can be tested without a GUI.
namespace TextDiff {

struct Diff
{
    QString text;
    QString error;
    bool ok = false;
};

// Returns a unified diff of `oldLines` vs `newLines` with `context` lines of
// surrounding context around each change. Very large inputs are refused with an
// error before the line matrix is allocated, so a comparison cannot exhaust
// memory on two ordinary-looking files.
Diff compare(const QStringList &oldLines, const QStringList &newLines, int context = 3);
Diff compare(const QString &oldText, const QString &newText, int context = 3);

// Convenience: splits text into lines and diffs it.
QString unifiedDiff(const QStringList &oldLines, const QStringList &newLines,
                    int context = 3);
QString unifiedDiff(const QString &oldText, const QString &newText, int context = 3);

// True when the bytes decode as UTF-8 without replacement characters.
bool isValidUtf8(const QByteArray &bytes);

// Decodes a comparison input. When the bytes are not UTF-8 and `latin1Fallback`
// is true, decodes as Latin-1 and reports that fallback through `usedFallback`.
// Returns an empty string when neither decoding is allowed.
QString decodeText(const QByteArray &bytes, bool latin1Fallback, bool *usedFallback = nullptr);

} // namespace TextDiff