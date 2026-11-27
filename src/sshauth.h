#pragma once

#include <QString>

// Settings for SFTP public-key authentication. Kept free of widgets and of
// libcurl so the validation rules can be tested on their own; the Curl backend
// applies the values to every handle it creates.
namespace SshAuth {

struct Options
{
    QString identityFile;   // private key, e.g. ~/.ssh/id_ed25519
    QString publicKeyFile;  // usually identityFile + ".pub"
    QString knownHostsFile; // enables host-key verification when set
    QString passphrase;     // for an encrypted key; never persisted

    bool isEmpty() const
    {
        return identityFile.isEmpty() && publicKeyFile.isEmpty()
            && knownHostsFile.isEmpty() && passphrase.isEmpty();
    }
};

// Returns the first existing default key in `sshDirectory` (id_ed25519,
// id_ecdsa, id_rsa, id_dsa), or an empty string.
QString findDefaultIdentity(const QString &sshDirectory);

// Convenience for the user's own ~/.ssh.
QString defaultIdentityFile();

// Returns `identityFile` + ".pub" when that file exists, otherwise empty.
QString publicKeyFor(const QString &identityFile);

// Checks that every configured file exists, is a regular readable file and
// that its path is free of control characters. Empty fields are allowed.
bool validate(const Options &options, QString *error = nullptr);

} // namespace SshAuth
