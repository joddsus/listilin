#include "settingsafety.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QUuid>

#include <QMetaType>

namespace {

bool isControlChar(QChar c)
{
    const ushort u = c.unicode();
    if (u == 0)
        return true;
    if (u < 0x20 && u != '\t' && u != '\n' && u != '\r')
        return true;
    if (u == 0x7f)
        return true;
    return false;
}

bool validString(const QString &s, QString *reason)
{
    if (s.size() > 65536) {
        if (reason)
            *reason = QStringLiteral("string is too long");
        return false;
    }
    for (const QChar c : s) {
        if (isControlChar(c)) {
            if (reason)
                *reason = QStringLiteral("string contains control characters");
            return false;
        }
    }
    return true;
}

bool validVariant(const QVariant &value, int depth, QString *reason)
{
    if (depth > 8) {
        if (reason)
            *reason = QStringLiteral("settings value is too deeply nested");
        return false;
    }

    switch (value.typeId()) {
    case QMetaType::Bool:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::Float:
        return true;
    case QMetaType::QString:
        return validString(value.toString(), reason);
    case QMetaType::QByteArray: {
        const QByteArray bytes = value.toByteArray();
        if (bytes.size() > 1024 * 1024) {
            if (reason)
                *reason = QStringLiteral("byte array is too large");
            return false;
        }
        return true;
    }
    case QMetaType::QStringList: {
        const QStringList list = value.toStringList();
        if (list.size() > 1000) {
            if (reason)
                *reason = QStringLiteral("string list is too long");
            return false;
        }
        for (const QString &s : list) {
            if (!validString(s, reason))
                return false;
        }
        return true;
    }
    case QMetaType::QVariantList: {
        const QVariantList list = value.toList();
        if (list.size() > 1000) {
            if (reason)
                *reason = QStringLiteral("list is too long");
            return false;
        }
        for (const QVariant &v : list) {
            if (!validVariant(v, depth + 1, reason))
                return false;
        }
        return true;
    }
    case QMetaType::QVariantMap: {
        const QVariantMap map = value.toMap();
        if (map.size() > 1000) {
            if (reason)
                *reason = QStringLiteral("map is too large");
            return false;
        }
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            if (!SettingsSafety::isValidKey(it.key())) {
                if (reason)
                    *reason = QStringLiteral("invalid key in settings map");
                return false;
            }
            if (!validVariant(it.value(), depth + 1, reason))
                return false;
        }
        return true;
    }
    default:
        if (reason)
            *reason = QStringLiteral("unsupported settings value type");
        return false;
    }
}

} // namespace

namespace SettingsSafety {

bool isValidKey(const QString &key)
{
    if (key.isEmpty() || key.size() > 256)
        return false;
    for (const QChar c : key) {
        if (isControlChar(c))
            return false;
    }
    const QStringList parts = key.split('/');
    for (const QString &part : parts) {
        if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral(".."))
            return false;
        for (const QChar c : part) {
            const bool ok = c.isLetterOrNumber() || c == '_' || c == '-' || c == '.';
            if (!ok)
                return false;
        }
    }
    return true;
}

bool isValidValue(const QVariant &value, QString *reason)
{
    return validVariant(value, 0, reason);
}

QVariant sanitizeValue(const QVariant &value)
{
    if (isValidValue(value))
        return value;

    switch (value.typeId()) {
    case QMetaType::QString: {
        QString s = value.toString();
        QString out;
        out.reserve(s.size());
        for (const QChar c : s) {
            if (!isControlChar(c))
                out.append(c);
        }
        if (out.size() > 65536)
            out = out.left(65536);
        return out;
    }
    case QMetaType::QStringList: {
        QStringList out;
        for (const QString &s : value.toStringList()) {
            QVariant v = s;
            out.append(sanitizeValue(v).toString());
        }
        return out;
    }
    default:
        return QVariant();
    }
}

bool canReadFile(const QString &path, QString *error)
{
    QFileInfo info(path);
    if (!info.exists())
        return true;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.close();
    return true;
}

bool canWriteFile(const QString &path, QString *error)
{
    QFileInfo info(path);
    const QDir parent(info.absolutePath());
    if (!parent.exists()) {
        if (error)
            *error = QStringLiteral("settings directory does not exist");
        return false;
    }
    if (!QFileInfo(parent.absolutePath()).isWritable()) {
        if (error)
            *error = QStringLiteral("settings directory is not writable");
        return false;
    }
    if (info.exists() && !info.isWritable()) {
        if (error)
            *error = QStringLiteral("settings file is not writable");
        return false;
    }
    return true;
}

QMap<QString, QVariant> readSettings(const QString &path, QString *error)
{
    QMap<QString, QVariant> result;
    if (!canReadFile(path, error))
        return result;

    QFileInfo info(path);
    if (!info.exists())
        return result;

    QSettings settings(path, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        if (error)
            *error = QStringLiteral("settings file could not be parsed");
        return result;
    }

    bool ignored = false;
    for (const QString &key : settings.allKeys()) {
        if (!isValidKey(key)) {
            ignored = true;
            continue;
        }
        const QVariant value = settings.value(key);
        if (!isValidValue(value)) {
            ignored = true;
            continue;
        }
        result.insert(key, value);
    }
    if (ignored && error && error->isEmpty())
        *error = QStringLiteral("some settings entries were ignored");
    return result;
}

bool writeSettingsAtomic(const QString &path, const QMap<QString, QVariant> &values,
                        QString *error)
{
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (!isValidKey(it.key())) {
            if (error)
                *error = QStringLiteral("invalid settings key: %1").arg(it.key());
            return false;
        }
        QString reason;
        if (!isValidValue(it.value(), &reason)) {
            if (error)
                *error = QStringLiteral("invalid settings value for %1: %2")
                             .arg(it.key(), reason);
            return false;
        }
    }

    if (!canWriteFile(path, error))
        return false;

    if (QFileInfo::exists(path) && !canReadFile(path, error))
        return false;

    const QDir parent(QFileInfo(path).absolutePath());
    const QString tempPath = parent.filePath(QStringLiteral(".%1.tmp")
                                                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    const QString backupPath = parent.filePath(QStringLiteral(".%1.bak")
                                                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));

    {
        QSettings temp(tempPath, QSettings::IniFormat);
        for (auto it = values.constBegin(); it != values.constEnd(); ++it)
            temp.setValue(it.key(), it.value());
        temp.sync();
        if (temp.status() != QSettings::NoError) {
            QFile::remove(tempPath);
            if (error)
                *error = QStringLiteral("could not write temporary settings file");
            return false;
        }
    }

    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::ReadOnly)) {
        QFile::remove(tempPath);
        if (error)
            *error = tempFile.errorString();
        return false;
    }
    tempFile.close();

    // Preserve the existing file mode where possible.
    if (QFileInfo::exists(path)) {
        const QFile::Permissions perms = QFileInfo(path).permissions();
        QFile::setPermissions(tempPath, perms);
    }

    bool hadOriginal = QFileInfo::exists(path);
    if (hadOriginal && !QFile::rename(path, backupPath)) {
        QFile::remove(tempPath);
        if (error)
            *error = QStringLiteral("could not back up existing settings file");
        return false;
    }

    if (!QFile::rename(tempPath, path)) {
        if (hadOriginal)
            QFile::rename(backupPath, path);
        QFile::remove(tempPath);
        if (error)
            *error = QStringLiteral("could not replace settings file");
        return false;
    }

    QFile::remove(backupPath);
    return true;
}

bool updateSettings(const QString &path, const QMap<QString, QVariant> &updates,
                    QString *error)
{
    for (auto it = updates.constBegin(); it != updates.constEnd(); ++it) {
        if (!isValidKey(it.key())) {
            if (error)
                *error = QStringLiteral("invalid settings key: %1").arg(it.key());
            return false;
        }
        QString reason;
        if (!isValidValue(it.value(), &reason)) {
            if (error)
                *error = QStringLiteral("invalid settings value for %1: %2")
                             .arg(it.key(), reason);
            return false;
        }
    }

    QMap<QString, QVariant> current = readSettings(path, error);
    if (error && !error->isEmpty() && !QFileInfo::exists(path)) {
        // Missing file is fine; readSettings returns empty.
    }
    // readSettings may set a non-fatal "ignored entries" message; clear it for
    // update operations.
    if (error)
        error->clear();

    if (QFileInfo::exists(path) && !canReadFile(path, error))
        return false;

    for (auto it = updates.constBegin(); it != updates.constEnd(); ++it)
        current.insert(it.key(), sanitizeValue(it.value()));

    return writeSettingsAtomic(path, current, error);
}

} // namespace SettingsSafety