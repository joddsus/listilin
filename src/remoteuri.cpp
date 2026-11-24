#include "remoteuri.h"

#include <QChar>

namespace {

bool hasControlChars(const QString &value)
{
    for (const QChar ch : value) {
        const ushort u = ch.unicode();
        if (u == 0 || u < 0x20 || u == 0x7f)
            return true;
    }
    return false;
}

} // namespace

namespace RemoteUri {

bool isSupportedScheme(const QString &scheme)
{
    const QString lower = scheme.toLower();
    return lower == QStringLiteral("ftp") || lower == QStringLiteral("ftpes")
        || lower == QStringLiteral("ftps") || lower == QStringLiteral("sftp")
        || lower == QStringLiteral("smb");
}

bool isValidHost(const QString &host)
{
    const QString value = host.trimmed();
    if (value.isEmpty() || value.size() > 255)
        return false;
    if (hasControlChars(value))
        return false;
    if (value.contains(QLatin1Char('/')) || value.contains(QLatin1Char('\\'))
        || value.contains(QLatin1Char('@')) || value.contains(QLatin1Char(':'))
        || value.contains(QLatin1Char(' '))) {
        return false;
    }
    if (value == QLatin1String(".") || value == QLatin1String(".."))
        return false;
    return true;
}

bool isValidPort(const QString &port, int *value)
{
    const QString trimmed = port.trimmed();
    if (trimmed.isEmpty()) {
        if (value)
            *value = 0;
        return true;
    }
    bool ok = false;
    const int number = trimmed.toInt(&ok);
    if (!ok || number < 1 || number > 65535)
        return false;
    if (value)
        *value = number;
    return true;
}

bool isSafePath(const QString &path)
{
    QString value = path.trimmed();
    if (value.isEmpty())
        return true;
    if (!value.startsWith(QLatin1Char('/')) && !value.startsWith(QLatin1Char('\\')))
        return false;
    value.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const QStringList parts = value.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == QLatin1String(".") || part == QLatin1String(".."))
            return false;
        if (part.contains(QChar::Null) || part.contains(QChar('\n'))
            || part.contains(QChar('\r')))
            return false;
        if (hasControlChars(part))
            return false;
    }
    return true;
}

QString normalizePath(const QString &path)
{
    QString value = path.trimmed();
    if (value.isEmpty())
        return QStringLiteral("/");
    value.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (!value.startsWith(QLatin1Char('/')))
        value.prepend(QLatin1Char('/'));
    while (value.size() > 1 && value.endsWith(QLatin1Char('/')))
        value.chop(1);
    return value;
}

QString build(const QString &scheme, const QString &host, const QString &port,
              const QString &user, const QString &password, const QString &path,
              QString *error)
{
    if (!isSupportedScheme(scheme)) {
        if (error)
            *error = QStringLiteral("Unsupported protocol: %1").arg(scheme);
        return QString();
    }
    if (!isValidHost(host)) {
        if (error)
            *error = QStringLiteral("Invalid host name.");
        return QString();
    }
    if (!isValidPort(port, nullptr)) {
        if (error)
            *error = QStringLiteral("Invalid port number.");
        return QString();
    }
    if (!user.isEmpty() && (user.contains(QLatin1Char('@')) || user.contains(QLatin1Char(':'))
                            || user.contains(QLatin1Char('/')) || hasControlChars(user))) {
        if (error)
            *error = QStringLiteral("Invalid user name.");
        return QString();
    }
    if (!password.isEmpty() && (password.contains(QLatin1Char('@'))
                                || password.contains(QLatin1Char('/')) || hasControlChars(password))) {
        if (error)
            *error = QStringLiteral("Invalid password.");
        return QString();
    }
    if (!isSafePath(path)) {
        if (error)
            *error = QStringLiteral("Unsafe remote path.");
        return QString();
    }

    QString uri = scheme.toLower() + QStringLiteral("://");
    if (!user.isEmpty()) {
        uri += user;
        if (!password.isEmpty())
            uri += QLatin1Char(':') + password;
        uri += QLatin1Char('@');
    }
    uri += host.trimmed();
    const QString trimmedPort = port.trimmed();
    if (!trimmedPort.isEmpty())
        uri += QLatin1Char(':') + trimmedPort;
    uri += normalizePath(path);
    return uri;
}

} // namespace RemoteUri