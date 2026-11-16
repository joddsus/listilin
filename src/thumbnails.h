#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QThreadPool>

// Thumbnails for entries that are not local files: archive members and files on
// FTP, SFTP and SMB shares. The delegate asks for a pixmap while painting, and
// this class answers from a cache or starts a background fetch. Nothing is ever
// downloaded on the UI thread, and nothing above a size limit is downloaded at
// all, so a folder full of camera photos on a slow server cannot stall the
// window.
class RemoteThumbnails : public QObject
{
    Q_OBJECT

public:
    static RemoteThumbnails &instance();

    // True when the name looks like an image this class will fetch.
    static bool isCandidate(const QString &name);

    // A cached thumbnail, or a null pixmap while the first fetch runs.
    QPixmap pixmap(const QString &uri, const QString &name, qint64 size);

    void clear();

signals:
    // Emitted on the UI thread once a thumbnail is in the cache.
    void ready(const QString &uri);

private:
    explicit RemoteThumbnails(QObject *parent = nullptr);

    QHash<QString, QPixmap> m_cache;
    QSet<QString> m_pending;
    QThreadPool m_pool;
    quint64 m_counter = 0;
};
