#pragma once

#include <QString>
#include <QStringList>

// Helpers for validating and sanitising file names and relative paths. Kept
// free of widgets so the rules can be tested without a GUI.
namespace PathSafety {

// Returns true when `name` is a single, safe path component: no separators,
// no "." or "..", no control or null characters, no reserved device names on
// Windows, and within a sane length limit.
bool isSafePathComponent(const QString &name);

// Returns true when `relativePath` contains only safe components and does not
// escape its root via ".." or absolute segments.
bool isSafeRelativePath(const QString &relativePath);

// Replaces unsafe characters in `name` with underscores and trims trailing
// dots/spaces (Windows). Returns an empty string when nothing safe remains.
QString sanitizePathComponent(const QString &name);

// Splits a relative path into its components using both separators.
QStringList splitPath(const QString &path);

// Returns true when `candidate` stays inside `baseDir` (or equals it) after
// lexical normalisation. Used to defeat path traversal and symlink-escape
// attacks when joining untrusted names (remote listings, archive entries) to
// a local destination directory.
bool staysWithin(const QString &baseDir, const QString &candidate);

// Joins `baseDir` and the single component `name` only when `name` is a safe
// path component and the result stays within `baseDir`. Returns an empty
// string (with `error` populated) otherwise.
QString safeJoin(const QString &baseDir, const QString &name, QString *error = nullptr);

} // namespace PathSafety