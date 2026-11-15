#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

#include "entry.h"

// A location in the unified namespace. Everything the program browses is a
// URI, which is what makes an archive, an FTP site and a local directory look
// identical to the rest of the program.
//
//   file:///home/ola/dokumenter
//   archive:///home/ola/pakke.zip!/src
//   ftp://bruker@host/pub
//   sftp://bruker@host/var/www
//   smb://host/delning
struct VfsUri
{
    QString scheme;    // file, archive, ftp, sftp, smb
    QString authority; // host[:port], or empty for local
    QString path;      // container path (archive file) or remote/local path
    QString inner;     // path inside an archive

    bool isValid() const { return !scheme.isEmpty(); }
    bool isLocal() const { return scheme == QLatin1String("file"); }
    bool isArchive() const { return scheme == QLatin1String("archive"); }
    bool isRemote() const { return !isLocal() && !isArchive(); }

    QString toString() const;
    QString toLocalPath() const;

    static VfsUri parse(const QString &text);
    static QString childUri(const QString &parentUri, const QString &name);
    static QString parentUri(const QString &uri);
    // True when this URI points at something that can be browsed as a folder
    // even though it is a file: zip, tar, 7z, rar, iso and friends.
    static bool looksLikeArchive(const QString &uri);
    static QString archiveUriFor(const QString &fileUri);
};

class VfsBackend
{
public:
    virtual ~VfsBackend() = default;

    virtual QString scheme() const = 0;
    virtual bool list(const VfsUri &uri, bool includeHidden, QVector<Entry> *entries,
                      QString *error) = 0;
    // Materialise a file into a local path. Used for opening and for copying
    // out of archives and remote locations.
    virtual bool fetch(const VfsUri &uri, const QString &targetFile, QString *error) = 0;
    virtual bool writable() const { return false; }

    // Writing. Defaults report read-only so a backend only has to implement
    // what it can actually do. URIs passed in are already parsed by the caller.
    virtual bool mkdir(const VfsUri &parent, const QString &name, QString *error)
    {
        Q_UNUSED(parent)
        Q_UNUSED(name)
        if (error)
            *error = QStringLiteral("This location is read-only.");
        return false;
    }
    virtual bool remove(const VfsUri &uri, bool recursive, QString *error)
    {
        Q_UNUSED(uri)
        Q_UNUSED(recursive)
        if (error)
            *error = QStringLiteral("This location is read-only.");
        return false;
    }
    virtual bool rename(const VfsUri &uri, const QString &newName, QString *newUri, QString *error)
    {
        Q_UNUSED(uri)
        Q_UNUSED(newName)
        Q_UNUSED(newUri)
        if (error)
            *error = QStringLiteral("This location is read-only.");
        return false;
    }
    virtual bool put(const VfsUri &targetDir, const QString &name, const QString &localFile,
                     QString *error)
    {
        Q_UNUSED(targetDir)
        Q_UNUSED(name)
        Q_UNUSED(localFile)
        if (error)
            *error = QStringLiteral("This location is read-only.");
        return false;
    }
};

// The single entry point to every backend. Callers never talk to a backend
// directly, which is what keeps the UI free of protocol knowledge.
class Vfs
{
public:
    static Vfs &instance();

    void addBackend(std::shared_ptr<VfsBackend> backend);
    VfsBackend *backendFor(const QString &scheme) const;

    bool list(const QString &uri, bool includeHidden, QVector<Entry> *entries, QString *error);
    bool fetch(const QString &uri, const QString &targetFile, QString *error);
    bool writable(const QString &uri) const;
    bool mkdir(const QString &parentUri, const QString &name, QString *error);
    bool remove(const QString &uri, bool recursive, QString *error);
    bool rename(const QString &uri, const QString &newName, QString *newUri, QString *error);
    bool put(const QString &targetUri, const QString &name, const QString &localFile, QString *error);
    QStringList schemes() const;

    // Convenience used by the UI and by drop targets.
    static QString localPathOf(const QString &uri);
    static bool isLocal(const QString &uri);

private:
    Vfs() = default;

    QHash<QString, std::shared_ptr<VfsBackend>> m_backends;
};

// Extracts the display name of the last component, used in path bars and
// window titles.
QString vfsLeafName(const QString &uri);
