#include "externaltools.h"

#include <QProcess>
#include <QRegularExpression>

namespace ExternalTools {

bool parse(const QString &entry, Tool *tool)
{
    const QStringList parts = entry.split(QLatin1Char('|'));
    if (parts.size() < 2)
        return false;
    if (!tool)
        return true;

    tool->name = parts.at(0).trimmed();
    tool->program = parts.at(1).trimmed();
    if (tool->name.isEmpty() || tool->program.isEmpty())
        return false;

    tool->arguments.clear();
    if (parts.size() >= 3) {
        const QString args = parts.mid(2).join(QLatin1Char('|'));
        tool->arguments = args.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    }
    return true;
}

QString serialise(const Tool &tool)
{
    return tool.name + QLatin1Char('|') + tool.program + QLatin1Char('|')
           + tool.arguments.join(QLatin1Char(' '));
}

QList<Tool> parseAll(const QStringList &entries)
{
    QList<Tool> result;
    for (const QString &entry : entries) {
        Tool tool;
        if (parse(entry, &tool))
            result.append(tool);
    }
    return result;
}

QStringList serialiseAll(const QList<Tool> &tools)
{
    QStringList result;
    for (const Tool &tool : tools)
        result.append(serialise(tool));
    return result;
}

} // namespace ExternalTools