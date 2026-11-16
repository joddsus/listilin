#include "thumbnails.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QMimeDatabase>

#include "vfs.h"

namespace {

// Above this size a thumbnail would cost more than it is worth: the point is a
// quick look at a listing, not a second download of every picture.
constexpr qint64 MaxFileSize = 8 * 1024 * 1024;

// Two fetches at a time keeps a slow share from being flooded, and leaves the
// thread pool free when the user scrolls past a hundred photographs.
constexpr int WorkerCount = 2;

QString cacheKey(const QString &uri, qint64 size)
{
    return uri + QLatin1Char('|') + QString::number(size);
}

} // namespace

RemoteThumbnails &RemoteThumbnails::instance()
{
    static RemoteThumbnails loader;
    return loader;
}

RemoteThumbnails::RemoteThumbnails(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(WorkerCount);
}

bool RemoteThumbnails::isCandidate(const QString &name)
{
    static const QStringList suffixes = {
        QStringLiteral("png"),  QStringLiteral("jpg"),  QStringLiteral("jpeg"),
        QStringLiteral("gif"),  QStringLiteral("bmp"),  QStringLiteral("webp"),
        QStringLiteral("tif"),  QStringLiteral("tiff"), QStringLiteral("ico"),
        QStringLiteral("ppm"),  QStringLiteral("pgm"),  QStringLiteral("xpm"),
        QStringLiteral("svg"),
    };

    if (name.isEmpty())
        return false;

    const QString suffix = QFileInfo(name).suffix().toLower();
    if (suffixes.contains(suffix))
        return true;

    // Formats with no useful suffix still deserve a chance, as long as the
    // MIME type from the extension says image. This stays cheap because it only
    // runs for names the shortcut list did not match.
    return QMimeDatabase().mimeTypeForFile(name, QMimeDatabase::MatchExtension)
        .name()
        .startsWith(QStringLiteral("image/"));
}

QPixmap RemoteThumbnails::pixmap(const QString &uri, const QString &name, qint64 size)
{
    if (size <= 0 || size > MaxFileSize || !isCandidate(name))
        return QPixmap();

    const QString key = cacheKey(uri, size);

    const auto cached = m_cache.constFind(key);
    if (cached != m_cache.constEnd())
        return *cached;

    if (m_pending.contains(key))
        return QPixmap();

    if (m_cache.size() >= 300)
        m_cache.clear();

    m_pending.insert(key);
    const QString tempPath = QDir::tempPath() + QStringLiteral("/lister-thumb-")
        + QString::number(++m_counter) + QStringLiteral("-")
        + QString::number(QCoreApplication::applicationPid());

    m_pool.start([this, uri, key, tempPath] {
        QImage image;

        QString error;
        if (Vfs::instance().fetch(uri, tempPath, &error)) {
            QImageReader reader(tempPath);
            reader.setAutoTransform(true);
            const QSize size = reader.size();
            if (size.isValid())
                reader.setScaledSize(size.scaled(QSize(384, 384), Qt::KeepAspectRatio));
            image = reader.read();
        }
        QFile::remove(tempPath);

        // Queued with `this` as the context: if the application is shutting
        // down the call is simply dropped instead of touching a dead cache.
        QMetaObject::invokeMethod(
            this,
            [this, key, image] {
                m_pending.remove(key);
                if (image.isNull())
                    return;

                if (m_cache.size() >= 300)
                    m_cache.clear();
                m_cache.insert(key, QPixmap::fromImage(image));
                emit ready(key.left(key.lastIndexOf(QLatin1Char('|'))));
            },
            Qt::QueuedConnection);
    });

    return QPixmap();
}

void RemoteThumbnails::clear()
{
    m_cache.clear();
}
