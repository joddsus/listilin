#include "archivepaths.h"

#include <QFileInfo>

namespace {

QString cleanName(const QString &raw)
{
    QString name = raw;
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (name.startsWith(QLatin1String("./")))
        name.remove(0, 2);
    while (name.startsWith(QLatin1Char('/')))
        name.remove(0, 1);
    while (name.endsWith(QLatin1Char('/')))
        name.chop(1);

    const QStringList parts = name.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == QLatin1String(".") || part == QLatin1String("..")
            || part.contains(QChar::Null) || part.contains(QChar('\n'))
            || part.contains(QChar('\r'))) {
            return QString();
        }
        if (part.size() == 2 && part.at(1) == QLatin1Char(':')
            && part.at(0).isLetter()) {
            return QString();
        }
    }

    return name;
}

bool validPath(const QString &path)
{
    if (path.isEmpty())
        return false;
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return false;
    for (const QString &part : parts) {
        if (part == QLatin1String(".") || part == QLatin1String("..")
            || part.contains(QChar::Null) || part.contains(QChar('\n'))
            || part.contains(QChar('\r'))) {
            return false;
        }
        if (part.size() == 2 && part.at(1) == QLatin1Char(':')
            && part.at(0).isLetter()) {
            return false;
        }
    }
    return true;
}

} // namespace

namespace ArchivePaths {

QString cleanEntryName(const QString &raw)
{
    return cleanName(raw);
}

bool isValidEntryPath(const QString &path)
{
    return validPath(path);
}

QStringList splitParts(const QString &path)
{
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (suffix.isEmpty() || path.size() <= suffix.size() + 1)
        return QStringList();
    const QString base = path.left(path.size() - suffix.size() - 1);

    if (suffix == QLatin1String("001")) {
        QStringList parts;
        for (int i = 1; i <= 999; ++i) {
            const QString part = base + QLatin1Char('.')
                + QString::number(i).rightJustified(3, QLatin1Char('0'));
            if (!QFileInfo::exists(part))
                break;
            parts.append(part);
        }
        return parts.size() > 1 ? parts : QStringList();
    }

    const bool tail = suffix == QLatin1String("zip");
    const bool span = suffix.size() == 3 && suffix.startsWith(QLatin1Char('z'))
        && suffix.at(1).isDigit() && suffix.at(2).isDigit();
    if (!tail && !span)
        return QStringList();

    QStringList parts;
    for (int i = 1; i <= 99; ++i) {
        const QString part = base + QStringLiteral(".z")
            + QString::number(i).rightJustified(2, QLatin1Char('0'));
        if (!QFileInfo::exists(part))
            break;
        parts.append(part);
    }

    if (parts.isEmpty())
        return QStringList();

    const QString last = base + QStringLiteral(".zip");
    if (QFileInfo::exists(last))
        parts.append(last);

    return parts.size() > 1 ? parts : QStringList();
}

QString detectWriteFormat(const QString &path, QString *filter)
{
    const QString lower = path.toLower();
    if (filter)
        *filter = QStringLiteral("none");

    if (lower.endsWith(QStringLiteral(".zip")))
        return QStringLiteral("zip");
    if (lower.endsWith(QStringLiteral(".tar.gz")) || lower.endsWith(QStringLiteral(".tgz"))) {
        if (filter) *filter = QStringLiteral("gzip");
        return QStringLiteral("pax");
    }
    if (lower.endsWith(QStringLiteral(".tar.bz2")) || lower.endsWith(QStringLiteral(".tbz2"))
        || lower.endsWith(QStringLiteral(".tbz"))) {
        if (filter) *filter = QStringLiteral("bzip2");
        return QStringLiteral("pax");
    }
    if (lower.endsWith(QStringLiteral(".tar.xz")) || lower.endsWith(QStringLiteral(".txz"))) {
        if (filter) *filter = QStringLiteral("xz");
        return QStringLiteral("pax");
    }
    if (lower.endsWith(QStringLiteral(".tar.zst")) || lower.endsWith(QStringLiteral(".tzst"))) {
        if (filter) *filter = QStringLiteral("zstd");
        return QStringLiteral("pax");
    }
    if (lower.endsWith(QStringLiteral(".tar.z")) || lower.endsWith(QStringLiteral(".taz"))) {
        if (filter) *filter = QStringLiteral("compress");
        return QStringLiteral("pax");
    }
    if (lower.endsWith(QStringLiteral(".tar")))
        return QStringLiteral("pax");
    if (lower.endsWith(QStringLiteral(".7z")))
        return QStringLiteral("7zip");
    if (lower.endsWith(QStringLiteral(".iso")))
        return QStringLiteral("iso9660");
    if (lower.endsWith(QStringLiteral(".ar")) || lower.endsWith(QStringLiteral(".a"))) {
        if (filter) *filter = QStringLiteral("none");
        return QStringLiteral("arbsd");
    }
    if (lower.endsWith(QStringLiteral(".cpio")))
        return QStringLiteral("cpio");
    if (!lower.contains(QLatin1Char('.')))
        return QStringLiteral("zip");
    return QString();
}

bool isWritablePath(const QString &path)
{
    return !detectWriteFormat(path, nullptr).isEmpty();
}

} // namespace ArchivePaths