#include "sshauth.h"

#include <QDir>
#include <QFileInfo>

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

bool checkFile(const QString &path, const QString &label, bool required, QString *error)
{
    if (path.isEmpty()) {
        if (!required)
            return true;
        if (error)
            *error = QStringLiteral("%1 is empty.").arg(label);
        return false;
    }
    if (hasControlChars(path)) {
        if (error)
            *error = QStringLiteral("%1 contains control characters.").arg(label);
        return false;
    }
    const QFileInfo info(path);
    if (!info.exists()) {
        if (error)
            *error = QStringLiteral("%1 does not exist: %2").arg(label, path);
        return false;
    }
    if (!info.isFile()) {
        if (error)
            *error = QStringLiteral("%1 is not a file: %2").arg(label, path);
        return false;
    }
    if (!info.isReadable()) {
        if (error)
            *error = QStringLiteral("%1 is not readable: %2").arg(label, path);
        return false;
    }
    return true;
}

} // namespace

namespace SshAuth {

QString findDefaultIdentity(const QString &sshDirectory)
{
    static const QStringList candidates = {
        QStringLiteral("id_ed25519"),
        QStringLiteral("id_ecdsa"),
        QStringLiteral("id_rsa"),
        QStringLiteral("id_dsa"),
    };
    const QDir directory(sshDirectory);
    for (const QString &name : candidates) {
        const QFileInfo info(directory.filePath(name));
        if (info.isFile())
            return info.absoluteFilePath();
    }
    return {};
}

QString defaultIdentityFile()
{
    return findDefaultIdentity(QDir::homePath() + QStringLiteral("/.ssh"));
}

QString publicKeyFor(const QString &identityFile)
{
    if (identityFile.isEmpty())
        return {};
    const QString candidate = identityFile + QStringLiteral(".pub");
    const QFileInfo info(candidate);
    return info.isFile() ? candidate : QString();
}

bool validate(const Options &options, QString *error)
{
    if (!checkFile(options.identityFile, QStringLiteral("SSH private key"), false, error))
        return false;
    if (!checkFile(options.publicKeyFile, QStringLiteral("SSH public key"), false, error))
        return false;
    if (!checkFile(options.knownHostsFile, QStringLiteral("Known hosts file"), false, error))
        return false;
    if (error)
        error->clear();
    return true;
}

} // namespace SshAuth
