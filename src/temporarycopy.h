#pragma once

#include <QString>

// Hands a copy of an archive member or remote file to an external program.
// Every call gets its own folder, so two documents with the same name from
// different archives never overwrite each other while they are open.
namespace TemporaryCopy {

QString ofUri(const QString &uri, QString *error = nullptr);

} // namespace TemporaryCopy
