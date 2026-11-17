#include "vfs_archive.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

#include <archive.h>
#include <archive_entry.h>

static QString (*g_passwordProvider)(const QString &archivePath) = nullptr;

// Process-local cache of passwords keyed by archive path. Reading an encrypted
// archive prompts once and stores the result here; creating or editing an
// archive reuses the cached value so the user is not asked again and the
// original encryption is retained across rebuilds.
static QHash<QString, QString> g_passwordCache;
static QMutex g_passwordCacheMutex;

void setArchivePasswordProvider(QString (*provider)(const QString &archivePath))
{
    g_passwordProvider = provider;
}

void setArchivePassword(const QString &archivePath, const QString &password)
{
    QMutexLocker locker(&g_passwordCacheMutex);
    g_passwordCache.insert(archivePath, password);
}

QString cachedArchivePassword(const QString &archivePath)
{
    QMutexLocker locker(&g_passwordCacheMutex);
    if (g_passwordCache.contains(archivePath))
        return g_passwordCache.value(archivePath);
    return QString();
}

static const char *passphraseCallback(struct archive *a, void *client_data)
{
    Q_UNUSED(a)
    if (!client_data)
        return nullptr;
    const QString *path = static_cast<const QString *>(client_data);

    QString pw;
    {
        QMutexLocker locker(&g_passwordCacheMutex);
        if (g_passwordCache.contains(*path)) {
            pw = g_passwordCache.value(*path);
        } else if (g_passwordProvider) {
            pw = g_passwordProvider(*path);
            g_passwordCache.insert(*path, pw);
        }
    }
    if (pw.isEmpty())
        return nullptr;
    // libarchive keeps the returned pointer; store in a thread-local buffer.
    static thread_local QByteArray storage;
    storage = pw.toUtf8();
    return storage.constData();
}

namespace {

QString cleanArchiveName(const QString &raw)
{
    QString name = raw;
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (name.startsWith(QLatin1String("./")))
        name.remove(0, 2);
    while (name.startsWith(QLatin1Char('/')))
        name.remove(0, 1);
    while (name.endsWith(QLatin1Char('/')))
        name.chop(1);
    return name;
}

// The ordered parts of a split archive, or an empty list for a normal one.
// Two naming schemes cover almost everything in the wild: PKZIP spans
// (.z01, .z02, ..., .zip) and the numbered 7z/rar spans (.001, .002, ...).
QStringList splitParts(const QString &path)
{
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (suffix.isEmpty() || path.size() <= suffix.size() + 1)
        return QStringList();
    const QString base = path.left(path.size() - suffix.size() - 1);

    if (suffix == QLatin1String("001")) {
        QStringList parts;
        for (int i = 1; i <= 999; ++i) {
            const QString part = base + QLatin1Char('.')
                + QString::number(i).rightJustified(3, QLatin1Char('0'));
            if (!QFileInfo::exists(part))
                break;
            parts.append(part);
        }
        return parts.size() > 1 ? parts : QStringList();
    }

    const bool tail = suffix == QLatin1String("zip");
    const bool span = suffix.size() == 3 && suffix.startsWith(QLatin1Char('z'))
        && suffix.at(1).isDigit() && suffix.at(2).isDigit();
    if (!tail && !span)
        return QStringList();

    QStringList parts;
    for (int i = 1; i <= 99; ++i) {
        const QString part = base + QStringLiteral(".z")
            + QString::number(i).rightJustified(2, QLatin1Char('0'));
        if (!QFileInfo::exists(part))
            break;
        parts.append(part);
    }

    if (parts.isEmpty())
        return QStringList();

    const QString last = base + QStringLiteral(".zip");
    if (QFileInfo::exists(last))
        parts.append(last);

    return parts.size() > 1 ? parts : QStringList();
}

} // namespace

ArchiveBackend::ArchiveBackend() = default;

void ArchiveBackend::invalidateCache(const QString &archivePath)
{
    QMutexLocker locker(&m_cacheMutex);
    // Cache keys are "path|mtime|size". Drop every entry belonging to this
    // archive so a write is reflected, but leave other archives' listings
    // (and their temporary split-archive scratch files) untouched.
    const QString prefix = archivePath + QLatin1Char('|');
    QStringList toRemove;
    for (const QString &key : m_cache.keys()) {
        if (key.startsWith(prefix))
            toRemove.append(key);
    }
    for (const QString &key : toRemove) {
        m_cache.remove(key);
        m_cacheOrder.removeAll(key);
    }
}

ArchiveBackend::~ArchiveBackend()
{
    QMutexLocker locker(&m_multipartMutex);
    qDeleteAll(m_multipartScratch);
    m_multipartScratch.clear();
}

bool ArchiveBackend::rejectMultipart(const QString &archivePath, QString *error) const
{
    if (splitParts(archivePath).isEmpty())
        return false;

    if (error) {
        *error = QStringLiteral("%1 is one part of a split archive. It can be browsed "
                                "and extracted, but not edited in place.")
                     .arg(QFileInfo(archivePath).fileName());
    }
    return true;
}

bool ArchiveBackend::openForRead(struct archive *handle, const QString &archivePath, QString *error)
{
    QString effective = archivePath;
    const QStringList parts = splitParts(archivePath);

    if (!parts.isEmpty()) {
        QMutexLocker locker(&m_multipartMutex);
        QString fingerprint;
        for (const QString &part : parts) {
            const QFileInfo info(part);
            fingerprint += part + QLatin1Char(':')
                + QString::number(info.lastModified().toSecsSinceEpoch()) + QLatin1Char(':')
                + QString::number(info.size()) + QLatin1Char(';');
        }

        const auto cached = m_multipartFiles.constFind(fingerprint);
        if (cached != m_multipartFiles.constEnd()) {
            effective = *cached;
        } else {
            // The parts are byte ranges of one archive, so joining them in order
            // gives libarchive exactly what it would have read from a single file.
            auto *scratch = new QTemporaryFile(QDir::tempPath()
                                              + QStringLiteral("/lister-split-XXXXXX"));
            if (!scratch->open()) {
                if (error)
                    *error = QStringLiteral("Could not create a temporary file for the "
                                            "split archive");
                delete scratch;
                return false;
            }

            for (const QString &part : parts) {
                QFile in(part);
                if (!in.open(QIODevice::ReadOnly)) {
                    if (error)
                        *error = QStringLiteral("Could not read %1").arg(part);
                    delete scratch;
                    return false;
                }

                while (!in.atEnd()) {
                    const QByteArray chunk = in.read(4 * 1024 * 1024);
                    if (chunk.isEmpty())
                        break;
                    if (scratch->write(chunk) != chunk.size()) {
                        if (error)
                            *error = QStringLiteral("Could not write the temporary copy of "
                                                    "the split archive");
                        delete scratch;
                        return false;
                    }
                }
            }

            scratch->flush();

            // Keep a few split sets around so stepping in and out of the same
            // archive does not join megabytes again on every click.
            if (m_multipartOrder.size() >= 3) {
                const QString oldest = m_multipartOrder.takeFirst();
                m_multipartFiles.remove(oldest);
                delete m_multipartScratch.take(oldest);
            }
            m_multipartOrder.append(fingerprint);
            m_multipartFiles.insert(fingerprint, scratch->fileName());
            m_multipartScratch.insert(fingerprint, scratch);
            effective = scratch->fileName();
        }
    }

    if (archive_read_open_filename(handle, QFile::encodeName(effective).constData(), 65536)
        == ARCHIVE_OK) {
        return true;
    }

    if (error) {
        const char *message = archive_error_string(handle);
        *error = message ? QString::fromUtf8(message) : QStringLiteral("Could not open the archive");
    }
    return false;
}

bool ArchiveBackend::itemsFor(const QString &archivePath, QVector<ArchiveItem> *items,
                              QString *error)
{
    QMutexLocker cacheLocker(&m_cacheMutex);
    const QFileInfo info(archivePath);
    const QString key = archivePath + QLatin1Char('|')
        + QString::number(info.lastModified().toSecsSinceEpoch()) + QLatin1Char('|')
        + QString::number(info.size());

    // Mtime/size-aware invalidation: drop any stale entries for the same
    // archive whose key no longer matches the current file. This keeps the
    // cache honest even when an explicit invalidateCache() call is missed
    // (e.g. the archive changed on disk between visits), and prevents dead
    // keys from accumulating as the archive is edited.
    const QString prefix = archivePath + QLatin1Char('|');
    QStringList stale;
    for (const QString &existing : m_cache.keys()) {
        if (existing.startsWith(prefix) && existing != key)
            stale.append(existing);
    }
    for (const QString &dead : stale) {
        m_cache.remove(dead);
        m_cacheOrder.removeAll(dead);
    }

    const auto cached = m_cache.constFind(key);
    if (cached != m_cache.constEnd()) {
        *items = *cached;
        return true;
    }

    struct archive *handle = archive_read_new();
    archive_read_support_filter_all(handle);
    archive_read_support_format_all(handle);
    QString pathHolder = archivePath;
    archive_read_set_passphrase_callback(handle, &pathHolder, passphraseCallback);

    if (!openForRead(handle, archivePath, error)) {
        archive_read_free(handle);
        return false;
    }

    QVector<ArchiveItem> collected;
    struct archive_entry *entry = nullptr;

    while (archive_read_next_header(handle, &entry) == ARCHIVE_OK) {
        const char *rawName = archive_entry_pathname(entry);
        if (!rawName) {
            archive_read_data_skip(handle);
            continue;
        }

        ArchiveItem item;
        item.path = cleanArchiveName(QString::fromUtf8(rawName));
        if (item.path.isEmpty()) {
            archive_read_data_skip(handle);
            continue;
        }

        const mode_t type = archive_entry_filetype(entry);
        item.isDir = type == AE_IFDIR || item.path.endsWith(QLatin1Char('/'));
        item.size = item.isDir ? 0 : archive_entry_size(entry);

        const time_t mtime = archive_entry_mtime(entry);
        if (mtime > 0)
            item.modified = QDateTime::fromSecsSinceEpoch(mtime);

        collected.append(item);
        archive_read_data_skip(handle);
    }

    archive_read_free(handle);

    // Keep the cache bounded: an archive per recently visited container is
    // plenty, and it makes stepping in and out of the same zip instant.
    if (m_cacheOrder.size() > 8) {
        m_cache.remove(m_cacheOrder.takeFirst());
    }
    m_cacheOrder.append(key);
    m_cache.insert(key, collected);

    *items = collected;
    return true;
}

bool ArchiveBackend::list(const VfsUri &uri, bool includeHidden, QVector<Entry> *entries,
                          QString *error)
{
    Q_UNUSED(includeHidden)

    QVector<ArchiveItem> items;
    if (!itemsFor(uri.path, &items, error))
        return false;

    QString prefix = uri.inner;
    while (prefix.startsWith(QLatin1Char('/')))
        prefix.remove(0, 1);
    while (prefix.endsWith(QLatin1Char('/')))
        prefix.chop(1);

    const QString prefixWithSlash = prefix.isEmpty() ? QString() : prefix + QLatin1Char('/');

    QHash<QString, Entry> byName;
    QStringList order;

    for (const ArchiveItem &item : items) {
        QString relative = item.path;

        if (!prefix.isEmpty()) {
            if (relative == prefix)
                continue;
            if (!relative.startsWith(prefixWithSlash))
                continue;
            relative = relative.mid(prefixWithSlash.size());
        }

        if (relative.isEmpty())
            continue;

        const int slash = relative.indexOf(QLatin1Char('/'));

        Entry entry;
        if (slash < 0) {
            entry.name = relative;
            entry.isDir = item.isDir;
            entry.size = item.size;
            entry.modified = item.modified;
        } else {
            // A nested path means this component is a directory even if the
            // archive has no explicit entry for it, which is common in zips.
            entry.name = relative.left(slash);
            entry.isDir = true;
        }

        const auto existing = byName.constFind(entry.name);
        if (existing == byName.constEnd()) {
            byName.insert(entry.name, entry);
            order.append(entry.name);
        } else if (entry.isDir && !existing->isDir) {
            byName.insert(entry.name, entry);
        }
    }

    entries->clear();
    entries->reserve(order.size());
    for (const QString &name : order)
        entries->append(byName.value(name));

    return true;
}

bool ArchiveBackend::fetch(const VfsUri &uri, const QString &targetFile, QString *error)
{
    QString wanted = uri.inner;
    while (wanted.startsWith(QLatin1Char('/')))
        wanted.remove(0, 1);

    struct archive *handle = archive_read_new();
    archive_read_support_filter_all(handle);
    archive_read_support_format_all(handle);
    QString pathHolder = uri.path;
    archive_read_set_passphrase_callback(handle, &pathHolder, passphraseCallback);

    if (!openForRead(handle, uri.path, error)) {
        archive_read_free(handle);
        return false;
    }

    bool found = false;
    struct archive_entry *entry = nullptr;

    while (archive_read_next_header(handle, &entry) == ARCHIVE_OK) {
        const char *rawName = archive_entry_pathname(entry);
        if (rawName && cleanArchiveName(QString::fromUtf8(rawName)) == wanted) {
            found = true;
            break;
        }
        archive_read_data_skip(handle);
    }

    if (!found) {
        if (error)
            *error = QStringLiteral("%1 does not exist in the archive").arg(wanted);
        archive_read_free(handle);
        return false;
    }

    QFile out(targetFile);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("Could not write %1").arg(targetFile);
        archive_read_free(handle);
        return false;
    }

    constexpr size_t chunkSize = 64 * 1024;
    QByteArray buffer(static_cast<int>(chunkSize), Qt::Uninitialized);
    bool ok = true;

    while (true) {
        const la_ssize_t read = archive_read_data(handle, buffer.data(), chunkSize);
        if (read == 0)
            break;
        if (read < 0) {
            if (error) {
                const char *message = archive_error_string(handle);
                *error = message ? QString::fromUtf8(message)
                                 : QStringLiteral("Feil under utpakking");
            }
            ok = false;
            break;
        }
        if (out.write(buffer.constData(), read) != read) {
            if (error)
                *error = QStringLiteral("Could not write to %1").arg(targetFile);
            ok = false;
            break;
        }
    }

    out.close();
    archive_read_free(handle);

    if (!ok)
        QFile::remove(targetFile);

    return ok;
}

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

#include <archive_entry.h>

namespace {

struct ArchiveEntry
{
    QString path;
    bool isDir = false;
    qint64 size = 0;
    QDateTime modified;
    QByteArray data;
};

QString detectFormat(const QString &path, QString *filter)
{
    const QString lower = path.toLower();
    if (filter)
        *filter = QStringLiteral("none");

    if (lower.endsWith(QStringLiteral(".zip")))
        return QStringLiteral("zip");
    if (lower.endsWith(QStringLiteral(".tar.gz")) || lower.endsWith(QStringLiteral(".tgz"))) {
        if (filter)
            *filter = QStringLiteral("gzip");
        return QStringLiteral("tar");
    }
    if (lower.endsWith(QStringLiteral(".tar.bz2")) || lower.endsWith(QStringLiteral(".tbz2"))
        || lower.endsWith(QStringLiteral(".tbz"))) {
        if (filter)
            *filter = QStringLiteral("bzip2");
        return QStringLiteral("tar");
    }
    if (lower.endsWith(QStringLiteral(".tar.xz")) || lower.endsWith(QStringLiteral(".txz"))) {
        if (filter)
            *filter = QStringLiteral("xz");
        return QStringLiteral("tar");
    }
    if (lower.endsWith(QStringLiteral(".tar.zst")) || lower.endsWith(QStringLiteral(".tzst"))) {
        if (filter)
            *filter = QStringLiteral("zstd");
        return QStringLiteral("tar");
    }
    if (lower.endsWith(QStringLiteral(".tar")))
        return QStringLiteral("tar");
    if (lower.endsWith(QStringLiteral(".7z")))
        return QStringLiteral("7zip");
    // Default to zip so a brand-new archive is usable.
    return QStringLiteral("zip");
}

bool readAllEntries(const QString &archivePath, QVector<ArchiveEntry> *entries, QString *error)
{
    entries->clear();

    if (!QFileInfo::exists(archivePath)) {
        return true; // new archive
    }

    struct archive *handle = archive_read_new();
    archive_read_support_filter_all(handle);
    archive_read_support_format_all(handle);
    QString pathHolder = archivePath;
    archive_read_set_passphrase_callback(handle, &pathHolder, passphraseCallback);

    if (archive_read_open_filename(handle, QFile::encodeName(archivePath).constData(), 65536)
        != ARCHIVE_OK) {
        if (error) {
            const char *message = archive_error_string(handle);
            *error = message ? QString::fromUtf8(message)
                             : QStringLiteral("Could not open the archive");
        }
        archive_read_free(handle);
        return false;
    }

    struct archive_entry *entry = nullptr;
    while (archive_read_next_header(handle, &entry) == ARCHIVE_OK) {
        const char *rawName = archive_entry_pathname(entry);
        if (!rawName) {
            archive_read_data_skip(handle);
            continue;
        }

        ArchiveEntry item;
        item.path = cleanArchiveName(QString::fromUtf8(rawName));
        if (item.path.isEmpty()) {
            archive_read_data_skip(handle);
            continue;
        }

        const mode_t type = archive_entry_filetype(entry);
        item.isDir = type == AE_IFDIR || item.path.endsWith(QLatin1Char('/'));
        if (item.isDir)
            item.path = item.path; // keep trailing? cleanArchiveName strips trailing
        item.size = item.isDir ? 0 : archive_entry_size(entry);

        const time_t mtime = archive_entry_mtime(entry);
        if (mtime > 0)
            item.modified = QDateTime::fromSecsSinceEpoch(mtime);

        if (!item.isDir) {
            QByteArray buffer;
            constexpr size_t chunkSize = 64 * 1024;
            QByteArray chunk(static_cast<int>(chunkSize), Qt::Uninitialized);
            while (true) {
                const la_ssize_t read = archive_read_data(handle, chunk.data(), chunkSize);
                if (read == 0)
                    break;
                if (read < 0) {
                    if (error) {
                        const char *message = archive_error_string(handle);
                        *error = message ? QString::fromUtf8(message)
                                         : QStringLiteral("Read failed");
                    }
                    archive_read_free(handle);
                    return false;
                }
                buffer.append(chunk.constData(), static_cast<int>(read));
            }
            item.data = buffer;
            item.size = buffer.size();
        } else {
            archive_read_data_skip(handle);
        }

        entries->append(item);
    }

    archive_read_free(handle);
    return true;
}

bool writeEntries(const QString &archivePath, const QVector<ArchiveEntry> &entries, QString *error)
{
    QString filterName;
    const QString formatName = detectFormat(archivePath, &filterName);

    struct archive *handle = archive_write_new();
    if (archive_write_set_format_by_name(handle, formatName.toUtf8().constData()) != ARCHIVE_OK) {
        if (error) {
            const char *message = archive_error_string(handle);
            *error = message ? QString::fromUtf8(message) : QStringLiteral("Unsupported format");
        }
        archive_write_free(handle);
        return false;
    }

    if (filterName != QLatin1String("none")) {
        if (archive_write_add_filter_by_name(handle, filterName.toUtf8().constData()) != ARCHIVE_OK) {
            if (error) {
                const char *message = archive_error_string(handle);
                *error = message ? QString::fromUtf8(message) : QStringLiteral("Unsupported filter");
            }
            archive_write_free(handle);
            return false;
        }
    }

    // Retain encryption across rebuilds when a password is known for this
    // archive (cached from a previous read or supplied at creation time).
    const QString password = cachedArchivePassword(archivePath);
    if (!password.isEmpty()) {
        archive_write_set_passphrase(handle, password.toUtf8().constData());
        if (formatName == QLatin1String("zip"))
            archive_write_set_options(handle, "zip:encryption=aes256");
    }

    const QString tempPath = archivePath + QStringLiteral(".tmp");
    if (archive_write_open_filename(handle, QFile::encodeName(tempPath).constData()) != ARCHIVE_OK) {
        if (error) {
            const char *message = archive_error_string(handle);
            *error = message ? QString::fromUtf8(message)
                             : QStringLiteral("Could not create the archive");
        }
        archive_write_free(handle);
        return false;
    }

    for (const ArchiveEntry &item : entries) {
        struct archive_entry *entry = archive_entry_new();
        const QByteArray pathBytes = item.path.toUtf8();
        archive_entry_set_pathname(entry, pathBytes.constData());

        if (item.isDir) {
            archive_entry_set_filetype(entry, AE_IFDIR);
            archive_entry_set_size(entry, 0);
        } else {
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_size(entry, item.data.size());
        }

        if (item.modified.isValid())
            archive_entry_set_mtime(entry, item.modified.toSecsSinceEpoch(), 0);

        if (archive_write_header(handle, entry) != ARCHIVE_OK) {
            if (error) {
                const char *message = archive_error_string(handle);
                *error = message ? QString::fromUtf8(message) : QStringLiteral("Write header failed");
            }
            archive_entry_free(entry);
            archive_write_close(handle);
            archive_write_free(handle);
            QFile::remove(tempPath);
            return false;
        }

        if (!item.isDir && !item.data.isEmpty()) {
            const la_ssize_t written = archive_write_data(handle, item.data.constData(),
                                                          static_cast<size_t>(item.data.size()));
            if (written < 0) {
                if (error) {
                    const char *message = archive_error_string(handle);
                    *error = message ? QString::fromUtf8(message) : QStringLiteral("Write data failed");
                }
                archive_entry_free(entry);
                archive_write_close(handle);
                archive_write_free(handle);
                QFile::remove(tempPath);
                return false;
            }
        }

        archive_entry_free(entry);
    }

    archive_write_close(handle);
    archive_write_free(handle);

    // Replace original.
    if (QFileInfo::exists(archivePath) && !QFile::remove(archivePath)) {
        if (error)
            *error = QStringLiteral("Could not replace %1").arg(archivePath);
        QFile::remove(tempPath);
        return false;
    }
    if (!QFile::rename(tempPath, archivePath)) {
        if (error)
            *error = QStringLiteral("Could not finalise %1").arg(archivePath);
        QFile::remove(tempPath);
        return false;
    }

    return true;
}

QString normaliseInnerPath(const QString &inner)
{
    QString value = inner;
    while (value.startsWith(QLatin1Char('/')))
        value.remove(0, 1);
    while (value.endsWith(QLatin1Char('/')) && value.size() > 1)
        value.chop(1);
    return value;
}

} // namespace

bool ArchiveBackend::mkdir(const VfsUri &parent, const QString &name, QString *error)
{
    if (rejectMultipart(parent.path, error))
        return false;

    QVector<ArchiveEntry> entries;
    if (!readAllEntries(parent.path, &entries, error))
        return false;

    const QString prefix = normaliseInnerPath(parent.inner);
    QString target = name;
    if (!prefix.isEmpty())
        target = prefix + QLatin1Char('/') + name;

    for (const ArchiveEntry &entry : entries) {
        if (entry.path == target)
            return true; // already exists
    }

    ArchiveEntry dir;
    dir.path = target;
    dir.isDir = true;
    entries.append(dir);

    if (!writeEntries(parent.path, entries, error))
        return false;

    invalidateCache(parent.path);
    return true;
}

bool ArchiveBackend::remove(const VfsUri &uri, bool recursive, QString *error)
{
    if (rejectMultipart(uri.path, error))
        return false;

    QVector<ArchiveEntry> entries;
    if (!readAllEntries(uri.path, &entries, error))
        return false;

    const QString target = normaliseInnerPath(uri.inner);
    if (target.isEmpty()) {
        if (error)
            *error = QStringLiteral("Cannot remove the archive root");
        return false;
    }

    QVector<ArchiveEntry> kept;
    for (const ArchiveEntry &entry : entries) {
        const bool isTarget = entry.path == target;
        const bool isChild = entry.path.startsWith(target + QLatin1Char('/'));
        if (isTarget || (recursive && isChild))
            continue;
        if (!recursive && isChild) {
            if (error)
                *error = QStringLiteral("Folder is not empty");
            return false;
        }
        kept.append(entry);
    }

    if (!writeEntries(uri.path, kept, error))
        return false;

    invalidateCache(uri.path);
    return true;
}

bool ArchiveBackend::rename(const VfsUri &uri, const QString &newName, QString *newUri, QString *error)
{
    if (rejectMultipart(uri.path, error))
        return false;

    QVector<ArchiveEntry> entries;
    if (!readAllEntries(uri.path, &entries, error))
        return false;

    const QString target = normaliseInnerPath(uri.inner);
    if (target.isEmpty()) {
        if (error)
            *error = QStringLiteral("Cannot rename the archive root");
        return false;
    }

    const int slash = target.lastIndexOf(QLatin1Char('/'));
    const QString parent = slash < 0 ? QString() : target.left(slash);
    const QString newPath = parent.isEmpty() ? newName : parent + QLatin1Char('/') + newName;

    for (ArchiveEntry &entry : entries) {
        if (entry.path == target) {
            entry.path = newPath;
        } else if (entry.path.startsWith(target + QLatin1Char('/'))) {
            entry.path = newPath + entry.path.mid(target.size());
        }
    }

    if (!writeEntries(uri.path, entries, error))
        return false;

    invalidateCache(uri.path);

    if (newUri) {
        VfsUri u = uri;
        u.inner = QLatin1Char('/') + newPath;
        *newUri = u.toString();
    }
    return true;
}

bool ArchiveBackend::put(const VfsUri &targetDir, const QString &name, const QString &localFile, QString *error)
{
    if (rejectMultipart(targetDir.path, error))
        return false;

    QVector<ArchiveEntry> entries;
    if (!readAllEntries(targetDir.path, &entries, error))
        return false;

    QFile in(localFile);
    if (!in.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Could not read %1").arg(localFile);
        return false;
    }
    const QByteArray data = in.readAll();
    in.close();

    const QString prefix = normaliseInnerPath(targetDir.inner);
    const QString target = prefix.isEmpty() ? name : prefix + QLatin1Char('/') + name;

    // Replace existing entry with same path.
    for (int i = 0; i < entries.size(); ++i) {
        if (entries[i].path == target) {
            entries[i].isDir = false;
            entries[i].data = data;
            entries[i].size = data.size();
            entries[i].modified = QDateTime::currentDateTime();
            if (!writeEntries(targetDir.path, entries, error))
                return false;
            invalidateCache(targetDir.path);
            return true;
        }
    }

    ArchiveEntry item;
    item.path = target;
    item.isDir = false;
    item.data = data;
    item.size = data.size();
    item.modified = QDateTime::currentDateTime();
    entries.append(item);

    if (!writeEntries(targetDir.path, entries, error))
        return false;

    invalidateCache(targetDir.path);
    return true;
}
