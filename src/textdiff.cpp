#include "textdiff.h"

#include <QObject>
#include <QStringConverter>
#include <QVector>
#include <algorithm>

namespace {

// Each matrix cell is an int; two million cells is about 8 MiB. Above that the
// comparison is refused instead of growing with the product of both files.
constexpr qint64 kMaxCells = 2000000;

} // namespace

namespace {

struct Edit {
    char type = ' '; // ' ', '-', '+'
    int oldNo = 0;
    int newNo = 0;
    QString text;
};

QVector<Edit> buildEdits(const QStringList &a, const QStringList &b)
{
    const int m = a.size();
    const int n = b.size();
    QVector<QVector<int>> dp(m + 1, QVector<int>(n + 1, 0));
    for (int i = 1; i <= m; ++i) {
        for (int j = 1; j <= n; ++j) {
            if (a.at(i - 1) == b.at(j - 1))
                dp[i][j] = dp[i - 1][j - 1] + 1;
            else
                dp[i][j] = qMax(dp[i - 1][j], dp[i][j - 1]);
        }
    }

    QVector<Edit> reversed;
    int i = m, j = n;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && a.at(i - 1) == b.at(j - 1)) {
            Edit e;
            e.type = ' ';
            e.text = a.at(i - 1);
            reversed.append(e);
            --i;
            --j;
        } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
            Edit e;
            e.type = '+';
            e.text = b.at(j - 1);
            reversed.append(e);
            --j;
        } else {
            Edit e;
            e.type = '-';
            e.text = a.at(i - 1);
            reversed.append(e);
            --i;
        }
    }
    std::reverse(reversed.begin(), reversed.end());

    // Assign line numbers.
    int oldLine = 1, newLine = 1;
    for (Edit &e : reversed) {
        e.oldNo = oldLine;
        e.newNo = newLine;
        if (e.type == '-')
            ++oldLine;
        else if (e.type == '+')
            ++newLine;
        else {
            ++oldLine;
            ++newLine;
        }
    }
    return reversed;
}

} // namespace

namespace TextDiff {

bool isValidUtf8(const QByteArray &bytes)
{
    const auto *p = reinterpret_cast<const unsigned char *>(bytes.constData());
    const int size = bytes.size();
    int i = 0;
    while (i < size) {
        const unsigned char c = p[i];
        if (c < 0x80) {
            ++i;
            continue;
        }

        int extra = 0;
        unsigned int code = 0;
        if ((c & 0xE0) == 0xC0) {
            extra = 1;
            code = c & 0x1F;
            if (code < 2) // overlong
                return false;
        } else if ((c & 0xF0) == 0xE0) {
            extra = 2;
            code = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            extra = 3;
            code = c & 0x07;
        } else {
            return false;
        }

        if (i + extra >= size)
            return false;
        for (int j = 1; j <= extra; ++j) {
            const unsigned char next = p[i + j];
            if ((next & 0xC0) != 0x80)
                return false;
            code = (code << 6) | (next & 0x3F);
        }
        if (extra == 2 && (code < 0x800 || (code >= 0xD800 && code <= 0xDFFF)))
            return false;
        if (extra == 3 && (code < 0x10000 || code > 0x10FFFF))
            return false;
        i += extra + 1;
    }
    return true;
}

QString decodeText(const QByteArray &bytes, bool latin1Fallback, bool *usedFallback)
{
    if (usedFallback)
        *usedFallback = false;

    if (isValidUtf8(bytes))
        return QString::fromUtf8(bytes);
    if (!latin1Fallback)
        return QString();
    if (usedFallback)
        *usedFallback = true;
    return QString::fromLatin1(bytes);
}

Diff compare(const QStringList &oldLines, const QStringList &newLines, int context)
{
    Diff result;
    if (context < 0)
        context = 0;

    const qint64 cells = (qint64(oldLines.size()) + 1) * (qint64(newLines.size()) + 1);
    if (cells > kMaxCells) {
        result.error = QObject::tr("These files are too large to compare line by line (%1 and %2 "
                                   "lines). Split them or use a dedicated diff tool.")
                           .arg(oldLines.size())
                           .arg(newLines.size());
        return result;
    }

    const QVector<Edit> edits = buildEdits(oldLines, newLines);

    // Mark edits that are within `context` lines of a change.
    QVector<char> include(edits.size(), 0);
    for (int k = 0; k < edits.size(); ++k) {
        if (edits[k].type == ' ')
            continue;
        for (int d = -context; d <= context; ++d) {
            const int idx = k + d;
            if (idx >= 0 && idx < edits.size())
                include[idx] = 1;
        }
    }

    QString out;
    int k = 0;
    while (k < edits.size()) {
        if (!include[k]) {
            ++k;
            continue;
        }
        int start = k;
        int end = k;
        while (end + 1 < edits.size() && include[end + 1])
            ++end;

        // Header.
        int oldStart = edits[start].oldNo;
        int newStart = edits[start].newNo;
        int oldCount = 0, newCount = 0;
        for (int x = start; x <= end; ++x) {
            if (edits[x].type != '+')
                ++oldCount;
            if (edits[x].type != '-')
                ++newCount;
        }
        out += QStringLiteral("@@ -%1,%2 +%3,%4 @@\n")
                   .arg(oldStart).arg(oldCount).arg(newStart).arg(newCount);

        for (int x = start; x <= end; ++x) {
            const Edit &e = edits[x];
            QChar prefix = (e.type == ' ') ? QLatin1Char(' ') : QLatin1Char(e.type);
            out += prefix;
            out += e.text;
            out += QLatin1Char('\n');
        }

        k = end + 1;
    }

    if (out.isEmpty())
        out = QStringLiteral("No differences.\n");
    result.text = out;
    result.ok = true;
    return result;
}

Diff compare(const QString &oldText, const QString &newText, int context)
{
    return compare(oldText.split(QLatin1Char('\n')), newText.split(QLatin1Char('\n')), context);
}

QString unifiedDiff(const QStringList &oldLines, const QStringList &newLines, int context)
{
    const Diff result = compare(oldLines, newLines, context);
    return result.ok ? result.text : QString();
}

QString unifiedDiff(const QString &oldText, const QString &newText, int context)
{
    return unifiedDiff(oldText.split(QLatin1Char('\n')), newText.split(QLatin1Char('\n')), context);
}

} // namespace TextDiff