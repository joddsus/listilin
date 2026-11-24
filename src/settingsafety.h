#pragma once

#include <QMap>
#include <QString>
#include <QVariant>

// Validation and atomic persistence helpers for QSettings INI files. Kept free
// of widgets so the rules can be tested without a GUI.
namespace SettingsSafety {

// Returns true for a settings key that is non-empty, reasonably short and free
// of control characters or traversal-like components.
bool isValidKey(const QString &key);

// Returns true when `value` is a supported, bounded settings value.
bool isValidValue(const QVariant &value, QString *reason = nullptr);

// Returns a safe copy of `value`, or an empty/zero value when it cannot be
// made safe.
QVariant sanitizeValue(const QVariant &value);

// True when `path` is missing or can be opened for reading.
bool canReadFile(const QString &path, QString *error = nullptr);

// True when `path`'s parent directory is writable and an existing file is not
// read-only.
bool canWriteFile(const QString &path, QString *error = nullptr);

// Reads and validates every entry from an INI settings file.
QMap<QString, QVariant> readSettings(const QString &path, QString *error = nullptr);

// Replaces `path` atomically with the validated settings. An existing but
// unreadable file is never overwritten.
bool writeSettingsAtomic(const QString &path, const QMap<QString, QVariant> &values,
                        QString *error = nullptr);

// Reads the current settings, applies `updates` and writes them atomically.
bool updateSettings(const QString &path, const QMap<QString, QVariant> &updates,
                    QString *error = nullptr);

} // namespace SettingsSafety