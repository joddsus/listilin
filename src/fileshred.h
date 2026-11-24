#pragma once

#include <QString>

// Secure deletion helpers for local files. Overwrites file contents before
// removing them, so recoverable data is not left on disk. Kept free of widgets
// so the behaviour can be tested without a GUI.
namespace FileShred {

// Overwrites `path` with `passes` passes of zeros (and a final random pass)
// before removing it. Returns false if the file could not be securely removed.
bool shredFile(const QString &path, int passes = 3, QString *error = nullptr);

// Securely removes a directory tree. Files are shredded; directories are
// removed afterwards.
bool shredTree(const QString &path, int passes = 3, QString *error = nullptr);

} // namespace FileShred