#include "pathsafety.h"
#include "vfs_smb.h"

#include <QByteArray>
#include <QFile>
#include <QStringList>

#ifndef Q_OS_WIN
#include <fcntl.h>
#endif

namespace {

constexpr quint32 SmbcDirType = 7;
constexpr quint32 SmbcFileType = 8;
constexpr quint32 SmbcLinkType = 9;
constexpr quint32 SmbcFileShareType = 3;

} // namespace

SmbBackend::SmbBackend()
{
    resolve();
}

SmbBackend::~SmbBackend() = default;

bool SmbBackend::resolve()
{
    if (m_tried)
        return m_init != nullptr;
    m_tried = true;

    static const QStringList candidates = {
        QStringLiteral("libsmbclient.so.0"),
        QStringLiteral("libsmbclient.so"),
        QStringLiteral("libsmbclient.dylib"),
    };

    for (const QString &name : candidates) {
        m_library.setFileName(name);
        if (!m_library.load())
            continue;
        break;
    }

    if (!m_library.isLoaded())
        return false;

    m_init = reinterpret_cast<int (*)(void *, int)>(m_library.resolve("smbc_init"));
    m_opendir = reinterpret_cast<void *(*)(const char *)>(m_library.resolve("smbc_opendir"));
    m_readdir = reinterpret_cast<Dirent *(*)(void *)>(m_library.resolve("smbc_readdir"));
    m_closedir = reinterpret_cast<int (*)(void *)>(m_library.resolve("smbc_closedir"));
    m_open = reinterpret_cast<int (*)(const char *, int, int)>(m_library.resolve("smbc_open"));
    m_read = reinterpret_cast<long (*)(int, void *, unsigned long)>(m_library.resolve("smbc_read"));
    m_write
        = reinterpret_cast<long (*)(int, const void *, unsigned long)>(m_library.resolve("smbc_write"));
    m_close = reinterpret_cast<int (*)(int)>(m_library.resolve("smbc_close"));

    m_mkdir = reinterpret_cast<int (*)(const char *, int)>(m_library.resolve("smbc_mkdir"));
    m_rmdir = reinterpret_cast<int (*)(const char *)>(m_library.resolve("smbc_rmdir"));
    m_unlink = reinterpret_cast<int (*)(const char *)>(m_library.resolve("smbc_unlink"));
    m_rename = reinterpret_cast<int (*)(const char *, const char *)>(m_library.resolve("smbc_rename"));

    if (!m_init || !m_opendir || !m_readdir || !m_closedir || !m_open || !m_read || !m_close) {
        m_library.unload();
        m_init = nullptr;
        return false;
    }

    m_init(nullptr, 0);
    return true;
}

QString SmbBackend::urlFor(const VfsUri &uri)
{
    const QString path = uri.path.isEmpty() ? QStringLiteral("/") : uri.path;
    return QStringLiteral("smb://") + uri.authority + path;
}

QString SmbBackend::joinUrl(const QString &base, const QString &name)
{
    QString url = base;
    if (!url.endsWith(QLatin1Char('/')))
        url += QLatin1Char('/');
    return url + name;
}

bool SmbBackend::writable() const
{
    SmbBackend *self = const_cast<SmbBackend *>(this);
    if (!self->resolve())
        return false;
    return self->m_mkdir && self->m_rmdir && self->m_unlink && self->m_rename && self->m_write;
}

bool SmbBackend::list(const VfsUri &uri, bool includeHidden, QVector<Entry> *entries,
                      QString *error)
{
    Q_UNUSED(includeHidden)

    if (!resolve()) {
        if (error)
            *error = QStringLiteral("SMB is not available: libsmbclient was not found");
        return false;
    }

    QString url = urlFor(uri);
    if (!url.endsWith(QLatin1Char('/')))
        url += QLatin1Char('/');

    void *dir = m_opendir(url.toUtf8().constData());
    if (!dir) {
        if (error)
            *error = QStringLiteral("Could not open %1").arg(url);
        return false;
    }

    entries->clear();

    while (Dirent *dirent = m_readdir(dir)) {
        const QString name = QString::fromUtf8(dirent->name,
                                               qMin<int>(dirent->nameLen, 4096));
        if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String("..")
            || !PathSafety::isSafePathComponent(name))
            continue;

        Entry entry;
        entry.name = name;
        entry.isDir = dirent->type == SmbcDirType || dirent->type == SmbcFileShareType
            || dirent->type == SmbcLinkType;
        entries->append(entry);
    }

    m_closedir(dir);
    return true;
}

bool SmbBackend::fetch(const VfsUri &uri, const QString &targetFile, QString *error)
{
    if (!resolve()) {
        if (error)
            *error = QStringLiteral("SMB is not available: libsmbclient was not found");
        return false;
    }

    const QByteArray url = urlFor(uri).toUtf8();
    const int handle = m_open(url.constData(), 0 /* O_RDONLY */, 0);
    if (handle < 0) {
        if (error)
            *error = QStringLiteral("Could not open %1").arg(QString::fromUtf8(url));
        return false;
    }

    QFile out(targetFile);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_close(handle);
        if (error)
            *error = QStringLiteral("Could not write %1").arg(targetFile);
        return false;
    }

    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    bool ok = true;

    while (true) {
        const long read = m_read(handle, buffer.data(), static_cast<unsigned long>(buffer.size()));
        if (read == 0)
            break;
        if (read < 0) {
            if (error)
                *error = QStringLiteral("Error while reading from %1").arg(QString::fromUtf8(url));
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
    m_close(handle);

    if (!ok)
        QFile::remove(targetFile);

    return ok;
}

bool SmbBackend::mkdir(const VfsUri &parent, const QString &name, QString *error)
{
    if (!resolve() || !m_mkdir) {
        if (error)
            *error = QStringLiteral("This SMB library cannot create folders.");
        return false;
    }

    const QString url = joinUrl(urlFor(parent), name);
    if (m_mkdir(url.toUtf8().constData(), 0755) != 0) {
        if (error)
            *error = QStringLiteral("Could not create %1").arg(url);
        return false;
    }
    return true;
}

bool SmbBackend::removeRecursive(const QString &url, QString *error)
{
    QString path = url;
    if (!path.endsWith(QLatin1Char('/')))
        path += QLatin1Char('/');

    void *dir = m_opendir(path.toUtf8().constData());
    if (dir) {
        QStringList children;
        while (Dirent *dirent = m_readdir(dir)) {
            const QString name = QString::fromUtf8(dirent->name,
                                                   qMin<int>(dirent->nameLen, 4096));
            if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String(".."))
                continue;
            children.append(name);
        }
        m_closedir(dir);

        for (const QString &child : children) {
            if (!removeRecursive(joinUrl(url, child), error))
                return false;
        }
    }

    if (m_rmdir && m_rmdir(url.toUtf8().constData()) == 0)
        return true;
    if (m_unlink && m_unlink(url.toUtf8().constData()) == 0)
        return true;

    if (error)
        *error = QStringLiteral("Could not delete %1").arg(url);
    return false;
}

bool SmbBackend::remove(const VfsUri &uri, bool recursive, QString *error)
{
    if (!resolve() || !m_unlink || !m_rmdir) {
        if (error)
            *error = QStringLiteral("This SMB library cannot delete files.");
        return false;
    }

    const QString url = urlFor(uri);

    if (!recursive) {
        if (m_unlink(url.toUtf8().constData()) == 0 || m_rmdir(url.toUtf8().constData()) == 0)
            return true;
        if (error)
            *error = QStringLiteral("Could not delete %1").arg(url);
        return false;
    }

    return removeRecursive(url, error);
}

bool SmbBackend::rename(const VfsUri &uri, const QString &newName, QString *newUri,
                        QString *error)
{
    if (!resolve() || !m_rename) {
        if (error)
            *error = QStringLiteral("This SMB library cannot rename files.");
        return false;
    }

    QString path = uri.path;
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    const QString parentPath = slash >= 0 ? path.left(slash) : QString();

    VfsUri parent = uri;
    parent.path = parentPath.isEmpty() ? QStringLiteral("/") : parentPath;
    parent.inner.clear();

    const QString from = urlFor(uri);
    const QString to = joinUrl(urlFor(parent), newName);

    if (m_rename(from.toUtf8().constData(), to.toUtf8().constData()) != 0) {
        if (error)
            *error = QStringLiteral("Could not rename %1").arg(from);
        return false;
    }

    if (newUri) {
        VfsUri renamed = parent;
        renamed.path = (parentPath.isEmpty() ? QString() : parentPath)
            + QLatin1Char('/') + newName;
        *newUri = renamed.toString();
    }
    return true;
}

bool SmbBackend::put(const VfsUri &targetDir, const QString &name, const QString &localFile,
                     QString *error)
{
    if (!resolve() || !m_write) {
        if (error)
            *error = QStringLiteral("This SMB library cannot write files.");
        return false;
    }

    QFile in(localFile);
    if (!in.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Could not read %1").arg(localFile);
        return false;
    }

    const QString url = joinUrl(urlFor(targetDir), name);
    const QByteArray urlBytes = url.toUtf8();

#ifdef Q_OS_WIN
    const int flags = 1 | 0x40 | 0x200; // O_WRONLY | O_CREAT | O_TRUNC
#else
    const int flags = O_WRONLY | O_CREAT | O_TRUNC;
#endif

    const int handle = m_open(urlBytes.constData(), flags, 0644);
    if (handle < 0) {
        if (error)
            *error = QStringLiteral("Could not create %1").arg(url);
        return false;
    }

    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    bool ok = true;
    while (true) {
        const qint64 read = in.read(buffer.data(), buffer.size());
        if (read == 0)
            break;
        if (read < 0) {
            if (error)
                *error = QStringLiteral("Could not read %1").arg(localFile);
            ok = false;
            break;
        }

        qint64 written = 0;
        while (written < read) {
            const long chunk = m_write(handle, buffer.constData() + written,
                                       static_cast<unsigned long>(read - written));
            if (chunk <= 0) {
                if (error)
                    *error = QStringLiteral("Could not write to %1").arg(url);
                ok = false;
                break;
            }
            written += chunk;
        }
        if (!ok)
            break;
    }

    m_close(handle);

    // A partial upload is worse than none: remove the half written file so the
    // share never shows something that looks complete.
    if (!ok && m_unlink)
        m_unlink(urlBytes.constData());

    return ok;
}
