#pragma once

#include <QString>
#include <QStringList>

// Helpers for archive entry paths, split archives and writable archive formats.
// Kept free of widgets so the rules can be tested without a GUI.
namespace ArchivePaths {

// Cleans an archive member name into a safe relative path, or returns an
// empty string when the name contains traversal or other unsafe components.
QString cleanEntryName(const QString &raw);

// Returns true when `path` is a safe archive member path.
bool isValidEntryPath(const QString &path);

// Returns the ordered parts of a split archive, or an empty list for a normal
// archive.
QStringList splitParts(const QString &path);

// Detects the libarchive write format for `path`. Returns an empty string when
// the format cannot be written by Listista (for example ARJ, CAB, LHA or RAR5).
// `filter` receives the compression filter name ("none", "gzip", ...).
QString detectWriteFormat(const QString &path, QString *filter = nullptr);

// Returns true when Listista can create or edit an archive with this path.
bool isWritablePath(const QString &path);

} // namespace ArchivePaths