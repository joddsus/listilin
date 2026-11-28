#include "layoutstore.h"

#include "vfs.h"

#include <QByteArray>
#include <QSettings>

namespace LayoutStore {

bool validName(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return false;
    if (trimmed.size() > 64)
        return false;
    for (const QChar ch : trimmed) {
        if (ch == QLatin1Char('/') || ch == QLatin1Char('\\')
            || ch == QLatin1Char(':') || ch == QLatin1Char('*')
            || ch == QLatin1Char('?') || ch == QLatin1Char('"')
            || ch == QLatin1Char('<') || ch == QLatin1Char('>')
            || ch == QLatin1Char('|'))
            return false;
    }
    return true;
}

QString keyFor(const QString &name)
{
    // A settings key has to be injective in the display name: "Min layout"
    // and "Min_layout" are two different layouts and must not share a slot.
    // Base64url avoids '/', which QSettings treats as a group separator, and
    // is stable for the same name. Older builds used a space->underscore key;
    // those layouts are still read by their serialized display name.
    QByteArray encoded = name.trimmed().toUtf8().toBase64(QByteArray::Base64UrlEncoding);
    while (encoded.endsWith('='))
        encoded.chop(1);
    return QStringLiteral("id-") + QString::fromLatin1(encoded);
}

QString serialize(const Layout &layout)
{
    QStringList lines;
    lines << QStringLiteral("name=%1").arg(layout.name);
    lines << QStringLiteral("panelCount=%1").arg(layout.panelCount);
    lines << QStringLiteral("folderTree=%1").arg(layout.folderTree ? 1 : 0);
    lines << QStringLiteral("places=%1").arg(layout.places ? 1 : 0);
    // A saved layout is written to settings and can end up in a screenshot or
    // a bug report. Store remote folders without their password.
    const QString source = Vfs::isLocal(layout.sourceUri)
        ? layout.sourceUri
        : VfsUri::redactCredentials(layout.sourceUri);
    const QString dest = Vfs::isLocal(layout.destUri)
        ? layout.destUri
        : VfsUri::redactCredentials(layout.destUri);
    lines << QStringLiteral("sourceUri=%1").arg(source);
    lines << QStringLiteral("destUri=%1").arg(dest);
    return lines.join(QLatin1Char('\n'));
}

bool deserialize(const QString &text, Layout *layout)
{
    if (!layout)
        return false;
    Layout result;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1);
        if (key == QLatin1String("name"))
            result.name = value;
        else if (key == QLatin1String("panelCount"))
            result.panelCount = value.toInt();
        else if (key == QLatin1String("folderTree"))
            result.folderTree = (value == QLatin1String("1") || value.toLower() == QLatin1String("true"));
        else if (key == QLatin1String("places"))
            result.places = (value == QLatin1String("1") || value.toLower() == QLatin1String("true"));
        else if (key == QLatin1String("sourceUri"))
            result.sourceUri = value;
        else if (key == QLatin1String("destUri"))
            result.destUri = value;
    }
    if (result.panelCount != 1 && result.panelCount != 2)
        result.panelCount = 1;
    if (!validName(result.name))
        return false;
    *layout = result;
    return true;
}

} // namespace LayoutStore