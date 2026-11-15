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

    QLibrary m_library;

    int (*m_init)(void *, int) = nullptr;
    void *(*m_opendir)(const char *) = nullptr;
    Dirent *(*m_readdir)(void *) = nullptr;
    int (*m_closedir)(void *) = nullptr;
    int (*m_open)(const char *, int, int) = nullptr;
    long (*m_read)(int, void *, unsigned long) = nullptr;
    int (*m_close)(int) = nullptr;
    bool m_tried = false;
};
