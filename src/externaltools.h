#pragma once

#include <QString>
#include <QStringList>

#include <QList>

// Configurable external tools launched from the Tools menu. Kept free of
// widgets so the parsing and serialisation rules can be tested directly.
namespace ExternalTools {

struct Tool
{
    QString name;
    QString program;
    QStringList arguments;
};

// Parses a settings entry of the form "name|program|arg1 arg2 ..." into a Tool.
// Returns false when the entry is malformed.
bool parse(const QString &entry, Tool *tool);

// Serialises a Tool back into the settings entry format.
QString serialise(const Tool &tool);

// Parses a list of settings entries, skipping malformed ones.
QList<Tool> parseAll(const QStringList &entries);

// Serialises a list of tools.
QStringList serialiseAll(const QList<Tool> &tools);

} // namespace ExternalTools