#pragma once

#include <QHash>
#include <QMutex>
#include <QStringList>
#include <QVector>

class QTemporaryFile;
struct archive;

#include "vfs.h"

// Reads any archive libarchive understands (zip, tar, 7z, rar, iso, cpio, and
// the compressed tar variants) as if it were a directory tree.
void setArchivePasswordProvider(QString (*provider)(const QString &archivePath));

class ArchiveBackend : public VfsBackend
{
public:
    ArchiveBackend();
    ~ArchiveBackend() override;

    QString scheme() const override { return QStringLiteral("archive"); }
    bool list(const VfsUri &uri, bool includeHidden, QVector<Entry> *entries,
              QString *error) override;
    bool fetch(const VfsUri &uri, const QString &targetFile, QString *error) override;

    bool writable() const override { return true; }
    bool mkdir(const VfsUri &parent, const QString &name, QString *error) override;
    bool remove(const VfsUri &uri, bool recursive, QString *error) override;
    bool rename(const VfsUri &uri, const QString &newName, QString *newUri, QString *error) override;
    bool put(const VfsUri &targetDir, const QString &name, const QString &localFile, QString *error) override;

private:
    struct ArchiveItem
    {
        QString path;
        bool isDir = false;
        qint64 size = 0;
        QDateTime modified;
    };

    bool itemsFor(const QString &archivePath, QVector<ArchiveItem> *items, QString *error);

    // Drops every cached listing for one archive. The cache key is
    // "path|mtime|size", so matching on the path prefix keeps other archives'
    // listings intact instead of clearing the whole cache after a write.
    void invalidateCache(const QString &archivePath);

    // Opens a libarchive reader on `archivePath`, joining split parts
    // (.z01/.z02/.zip and .001/.002) into one temporary file first. Sets the
    // error and returns false when neither the archive nor its parts open.
    bool openForRead(struct archive *handle, const QString &archivePath, QString *error);
    bool rejectMultipart(const QString &archivePath, QString *error) const;

    QHash<QString, QVector<ArchiveItem>> m_cache;
    QStringList m_cacheOrder;
    QMutex m_cacheMutex;

    // Concatenated split archives, keyed by the parts' sizes and timestamps.
    // Listings, previews and thumbnail fetches all pass through here, so the
    // map and the temporary files behind it are guarded.
    QMutex m_multipartMutex;
    QHash<QString, QString> m_multipartFiles;
    QHash<QString, QTemporaryFile *> m_multipartScratch;
    QStringList m_multipartOrder;
};
