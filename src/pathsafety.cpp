#include "pathsafety.h"

#include <QDir>
#include <QSet>

namespace {

const QSet<QString> &reservedNames()
{
    static const QSet<QString> names = {
        QStringLiteral("CON"), QStringLiteral("PRN"), QStringLiteral("AUX"),
        QStringLiteral("NUL"), QStringLiteral("COM1"), QStringLiteral("COM2"),
        QStringLiteral("COM3"), QStringLiteral("COM4"), QStringLiteral("COM5"),
        QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"),
        QStringLiteral("LPT3"), QStringLiteral("LPT4"), QStringLiteral("LPT5"),
        QStringLiteral("LPT6"), QStringLiteral("LPT7"), QStringLiteral("LPT8"),
        QStringLiteral("LPT9")};
    return names;
}

bool hasControlChars(const QString &value)
{
    for (const QChar ch : value) {
        const ushort u = ch.unicode();
        if (u == 0 || (u < 0x20) || u == 0x7f)
            return true;
    }
    return false;
}

} // namespace

namespace PathSafety {

QStringList splitPath(const QString &path)
{
    QString normalized = path;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
}

bool isSafePathComponent(const QString &name)
{
    if (name.isEmpty() || name.size() > 255)
        return false;
    if (name == QStringLiteral(".") || name == QStringLiteral(".."))
        return false;
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')))
        return false;
    if (hasControlChars(name))
        return false;
    if (name.endsWith(QLatin1Char(' ')) || name.endsWith(QLatin1Char('.')))
        return false;

    QString upper = name.toUpper();
    const int dot = upper.indexOf(QLatin1Char('.'));
    if (dot >= 0)
        upper = upper.left(dot);
    if (reservedNames().contains(upper))
        return false;

    return true;
}

bool isSafeRelativePath(const QString &relativePath)
{
    if (relativePath.isEmpty())
        return true;
    if (relativePath.startsWith(QLatin1Char('/')) || relativePath.startsWith(QLatin1Char('\\')))
        return false;
    const QStringList parts = splitPath(relativePath);
    for (const QString &part : parts) {
        if (!isSafePathComponent(part))
            return false;
    }
    return true;
}

QString sanitizePathComponent(const QString &name)
{
    QString result;
    result.reserve(name.size());
    for (const QChar ch : name) {
        const ushort u = ch.unicode();
        if (u == 0 || u < 0x20 || u == 0x7f || ch == QLatin1Char('/') || ch == QLatin1Char('\\')
            || ch == QLatin1Char(':') || ch == QLatin1Char('*') || ch == QLatin1Char('?')
            || ch == QLatin1Char('"') || ch == QLatin1Char('<') || ch == QLatin1Char('>')
            || ch == QLatin1Char('|')) {
            result.append(QLatin1Char('_'));
        } else {
            result.append(ch);
        }
    }
    // Trim trailing dots/spaces (Windows restriction).
    while (!result.isEmpty() && (result.endsWith(QLatin1Char('.')) || result.endsWith(QLatin1Char(' '))))
        result.chop(1);
    if (result == QStringLiteral(".") || result == QStringLiteral(".."))
        result.clear();
    // Reserved names.
    QString upper = result.toUpper();
    const int dot = upper.indexOf(QLatin1Char('.'));
    if (dot >= 0)
        upper = upper.left(dot);
    if (reservedNames().contains(upper) && !result.isEmpty()) {
        result.append(QLatin1Char('_'));
    }
    if (result.isEmpty())
        return QString();
    if (result.size() > 255)
        result = result.left(255);
    return result;
}

bool staysWithin(const QString &baseDir, const QString &candidate)
{
    if (baseDir.isEmpty() || candidate.isEmpty())
        return false;

    const QString base = QDir::cleanPath(baseDir);
    const QString target = QDir::cleanPath(candidate);

    if (target == base)
        return true;
    if (target.startsWith(base + QLatin1Char('/')))
        return true;
    return false;
}

QString safeJoin(const QString &baseDir, const QString &name, QString *error)
{
    if (!isSafePathComponent(name)) {
        if (error)
            *error = QStringLiteral("Unsafe path component: %1").arg(name);
        return QString();
    }

    const QString joined = QDir(baseDir).filePath(name);
    const QString cleaned = QDir::cleanPath(joined);
    if (!staysWithin(baseDir, cleaned)) {
        if (error)
            *error = QStringLiteral("Path escapes destination: %1").arg(name);
        return QString();
    }
    return cleaned;
}

} // namespace PathSafety