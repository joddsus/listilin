#pragma once

#include <QByteArray>
#include <QString>

// Replace a file atomically with a fully written sibling.
//
// Publishing by renaming a staged file over the target means there is never a
// moment where the destination name is missing: either the old file is still
// there, or the new one has taken its place. A crash in between leaves one of
// the two complete versions, never a truncated file.
namespace AtomicFile {

// A unique hidden path next to `target`, so staging and publishing happen on
// the same filesystem (rename is only atomic within one).
QString stagingPath(const QString &target, const QString &tag = QStringLiteral("copy"));

// Moves `staged` onto `target`, replacing an existing regular file or symlink.
// Refuses to replace a directory. On failure the old target is left untouched.
bool publish(const QString &staged, const QString &target, QString *error = nullptr);

// Writes `data` to `target` through a staged sibling. Keeps the permissions of
// an existing target when there is one.
bool write(const QString &target, const QByteArray &data, QString *error = nullptr);

} // namespace AtomicFile
