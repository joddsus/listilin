#include "renamebatch.h"

#include <QSet>

namespace RenameBatch {

namespace {

bool isDotOnly(const QString &text)
{
    for (const QChar ch : text) {
        if (ch != QLatin1Char('.'))
            return false;
    }
    return true;
}

void splitLeafName(const QString &name, QString *stem, QString *extension)
{
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    // A leading dot marks a hidden file, and a trailing dot is part of the
    // name; neither starts an extension. The same goes for a name that starts
    // with several dots, such as "..foo": it is hidden, not an empty stem
    // plus the "foo" extension.
    if (dot <= 0 || dot == name.size() - 1 || isDotOnly(name.left(dot))) {
        *stem = name;
        extension->clear();
        return;
    }

    *stem = name.left(dot);
    *extension = name.mid(dot + 1);
}

} // namespace

Plan plan(const QStringList &names, const Options &options)
{
    Plan result;
    result.currentNames = names;
    result.newNames.reserve(names.size());
    result.duplicate.fill(false, names.size());
    result.unchanged.fill(false, names.size());

    QSet<QString> seen;
    for (qsizetype i = 0; i < names.size(); ++i) {
        QString stem;
        QString extension;
        splitLeafName(names.at(i), &stem, &extension);

        if (!options.find.isEmpty())
            stem.replace(options.find, options.replace);
        if (!options.prefix.isEmpty())
            stem.prepend(options.prefix);
        if (!options.suffix.isEmpty())
            stem.append(options.suffix);
        if (options.numbering)
            stem += QString::number(options.startNumber + i);

        QString newName = stem;
        if (options.keepExtension && !extension.isEmpty())
            newName += QLatin1Char('.') + extension;
        if (newName.isEmpty())
            newName = names.at(i);

        result.newNames.append(newName);
        result.duplicate[i] = seen.contains(newName);
        result.unchanged[i] = (newName == names.at(i));
        seen.insert(newName);
    }

    return result;
}

int Plan::renameCount() const
{
    int count = 0;
    for (const bool kept : unchanged) {
        if (!kept)
            ++count;
    }
    return count;
}

} // namespace RenameBatch
