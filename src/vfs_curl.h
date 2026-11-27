#pragma once

#include <QString>

#include "sshauth.h"
#include "vfs.h"

// Process-wide SFTP key settings, applied to every handle the Curl backend
// creates. The Connect dialog updates them; paths are persisted by the caller,
// while an optional key passphrase only ever lives in memory.
void setSshOptions(const SshAuth::Options &options);

// FTP, FTPS and SFTP through libcurl, which already knows how to talk to all
// of them and how to authenticate against them. SFTP can use an explicit
// private key, its public key, a passphrase and a known_hosts file. "ftps" is the implicit TLS
// flavour; "ftpes" asks libcurl to upgrade a normal FTP connection with AUTH
// TLS, which is what most "FTPS" servers in the wild speak.
class CurlBackend : public VfsBackend
{
public:
    explicit CurlBackend(QString scheme);

    QString scheme() const override { return m_scheme; }
    bool list(const VfsUri &uri, bool includeHidden, QVector<Entry> *entries,
              QString *error) override;
    bool fetch(const VfsUri &uri, const QString &targetFile, QString *error) override;
    bool writable() const override { return true; }
    bool mkdir(const VfsUri &parent, const QString &name, QString *error) override;
    bool remove(const VfsUri &uri, bool recursive, QString *error) override;
    bool rename(const VfsUri &uri, const QString &newName, QString *newUri, QString *error) override;
    bool put(const VfsUri &targetDir, const QString &name, const QString &localFile, QString *error) override;

private:
    QString m_scheme;
};
