#pragma once

#include <QLibrary>
#include <QString>

#include "vfs.h"

// SMB/CIFS shares through libsmbclient. The library is loaded at runtime so the
// program still builds and runs on systems without Samba installed; the scheme
// reports a clear message instead of disappearing.
class SmbBackend : public VfsBackend
{
public:
    SmbBackend();
    ~SmbBackend() override;

    QString scheme() const override { return QStringLiteral("smb"); }
    bool list(const VfsUri &uri, bool includeHidden, QVector<Entry> *entries,
              QString *error) override;
    bool fetch(const VfsUri &uri, const QString &targetFile, QString *error) override;
    bool writable() const override;
    bool mkdir(const VfsUri &parent, const QString &name, QString *error) override;
    bool remove(const VfsUri &uri, bool recursive, QString *error) override;
    bool rename(const VfsUri &uri, const QString &newName, QString *newUri, QString *error) override;
    bool put(const VfsUri &targetDir, const QString &name, const QString &localFile,
             QString *error) override;

private:
    struct Dirent
    {
        quint32 type;
        quint32 dirLen;
        quint32 commentLen;
        char *comment;
        quint16 nameLen;
        char name[1];
    };

    bool resolve();
    static QString urlFor(const VfsUri &uri);
    static QString joinUrl(const QString &base, const QString &name);
    bool removeRecursive(const QString &url, QString *error);

    QLibrary m_library;

    int (*m_init)(void *, int) = nullptr;
    void *(*m_opendir)(const char *) = nullptr;
    Dirent *(*m_readdir)(void *) = nullptr;
    int (*m_closedir)(void *) = nullptr;
    int (*m_open)(const char *, int, int) = nullptr;
    long (*m_read)(int, void *, unsigned long) = nullptr;
    long (*m_write)(int, const void *, unsigned long) = nullptr;
    int (*m_close)(int) = nullptr;

    // Writing is optional even when reading works: libsmbclient can be built
    // without it, and an old library may not export every symbol.
    int (*m_mkdir)(const char *, int) = nullptr;
    int (*m_rmdir)(const char *) = nullptr;
    int (*m_unlink)(const char *) = nullptr;
    int (*m_rename)(const char *, const char *) = nullptr;

    bool m_tried = false;
};
