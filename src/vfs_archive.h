#pragma once

#include <QHash>
#include <QVector>

#include "vfs.h"

// Reads any archive libarchive understands (zip, tar, 7z, rar, iso, cpio, and
// the compressed tar variants) as if it were a directory tree.
void setArchivePasswordProvider(QString (*provider)(const QString &archivePath));

class ArchiveBackend : public VfsBackend
{
public:
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

    const QVector<ArchiveItem> *itemsFor(const QString &archivePath, QString *error);

    QHash<QString, QVector<ArchiveItem>> m_cache;
    QStringList m_cacheOrder;
};
