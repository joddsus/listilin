#include "vfs_smb.h"

#include <QByteArray>
#include <QFile>
#include <QStringList>

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
    m_close = reinterpret_cast<int (*)(int)>(m_library.resolve("smbc_close"));

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
        if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String(".."))
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
