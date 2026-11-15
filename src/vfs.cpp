#include "vfs.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace {

QString joinPath(const QString &base, const QString &name)
{
    if (base.isEmpty() || base == QLatin1String("/"))
        return QLatin1Char('/') + name;

    QString trimmed = base;
    while (trimmed.endsWith(QLatin1Char('/')))
        trimmed.chop(1);

    return trimmed + QLatin1Char('/') + name;
}

QString parentPath(const QString &path)
{
    if (path.isEmpty() || path == QLatin1String("/"))
        return QStringLiteral("/");

    QString trimmed = path;
    while (trimmed.size() > 1 && trimmed.endsWith(QLatin1Char('/')))
        trimmed.chop(1);

    const int slash = trimmed.lastIndexOf(QLatin1Char('/'));
    if (slash <= 0)
        return QStringLiteral("/");
    return trimmed.left(slash);
}

QString normaliseInner(const QString &inner)
{
    if (inner.isEmpty())
        return QString();
    QString value = inner;
    if (!value.startsWith(QLatin1Char('/')))
        value.prepend(QLatin1Char('/'));
    while (value.endsWith(QLatin1Char('/')) && value.size() > 1)
        value.chop(1);
    return value;
}

} // namespace

QString VfsUri::toString() const
{
    if (scheme == QLatin1String("file"))
        return QStringLiteral("file://") + path;

    if (scheme == QLatin1String("archive")) {
        const QString innerPath = inner.isEmpty() ? QString() : QStringLiteral("!") + inner;
        return QStringLiteral("archive://") + path + innerPath;
    }

    return scheme + QStringLiteral("://") + authority + path;
}

QString VfsUri::toLocalPath() const
{
    return isLocal() ? path : QString();
}

VfsUri VfsUri::parse(const QString &text)
{
    VfsUri uri;
    const QString value = text.trimmed();
    if (value.isEmpty())
        return uri;

    const int marker = value.indexOf(QStringLiteral("://"));
    if (marker < 0) {
        // A bare path is a local path; that keeps the address bar usable for
        // people who type /home/ola instead of a URI.
        uri.scheme = QStringLiteral("file");
        uri.path = QDir::cleanPath(value.startsWith(QLatin1Char('/'))
                                       ? value
                                       : QDir::current().absoluteFilePath(value));
        return uri;
    }

    uri.scheme = value.left(marker).toLower();

    QString remainder = value.mid(marker + 3);

    if (uri.scheme == QLatin1String("file")) {
        uri.path = QDir::cleanPath(remainder.isEmpty() ? QStringLiteral("/") : remainder);
        return uri;
    }

    const int bang = remainder.indexOf(QLatin1Char('!'));
    if (uri.scheme == QLatin1String("archive") || bang >= 0) {
        if (bang >= 0) {
            uri.inner = normaliseInner(remainder.mid(bang + 1));
            remainder = remainder.left(bang);
        }
        uri.scheme = QStringLiteral("archive");
        uri.path = QDir::cleanPath(remainder.startsWith(QLatin1Char('/'))
                                       ? remainder
                                       : QLatin1Char('/') + remainder);
        return uri;
    }

    const int slash = remainder.indexOf(QLatin1Char('/'));
    if (slash < 0) {
        uri.authority = remainder;
        uri.path = QStringLiteral("/");
    } else {
        uri.authority = remainder.left(slash);
        uri.path = remainder.mid(slash);
        if (uri.path.isEmpty())
            uri.path = QStringLiteral("/");
    }

    return uri;
}

QString VfsUri::childUri(const QString &parentUri, const QString &name)
{
    VfsUri uri = parse(parentUri);

    if (uri.isArchive()) {
        const QString inner = joinPath(uri.inner, name);
        uri.inner = inner;
        return uri.toString();
    }

    uri.path = joinPath(uri.path, name);
    return uri.toString();
}

QString VfsUri::parentUri(const QString &uriText)
{
    VfsUri uri = parse(uriText);

    if (uri.isArchive()) {
        if (!uri.inner.isEmpty() && uri.inner != QLatin1String("/")) {
            const QString parent = parentPath(uri.inner);
            uri.inner = parent == QLatin1String("/") ? QString() : parent;
            return uri.toString();
        }

        // At the archive root: step out to the directory holding the archive.
        VfsUri local;
        local.scheme = QStringLiteral("file");
        local.path = parentPath(uri.path);
        return local.toString();
    }

    if (uri.isLocal()) {
        const QString parent = parentPath(uri.path);
        if (parent == uri.path)
            return uri.toString();
        uri.path = parent;
        return uri.toString();
    }

    if (uri.path.isEmpty() || uri.path == QLatin1String("/"))
        return uri.toString();

    uri.path = parentPath(uri.path);
    return uri.toString();
}

bool VfsUri::looksLikeArchive(const QString &uriText)
{
    const VfsUri uri = parse(uriText);
    if (!uri.isLocal())
        return false;

    const QString lower = uri.path.toLower();
    static const QStringList suffixes = {
        QStringLiteral(".zip"),   QStringLiteral(".tar"),   QStringLiteral(".tar.gz"),
        QStringLiteral(".tgz"),   QStringLiteral(".tar.bz2"), QStringLiteral(".tbz"),
        QStringLiteral(".tbz2"),  QStringLiteral(".tar.xz"), QStringLiteral(".txz"),
        QStringLiteral(".tar.zst"), QStringLiteral(".tzst"), QStringLiteral(".7z"),
        QStringLiteral(".rar"),   QStringLiteral(".iso"),    QStringLiteral(".cpio"),
        QStringLiteral(".ar"),    QStringLiteral(".deb"),    QStringLiteral(".rpm"),
        QStringLiteral(".lha"),   QStringLiteral(".lzh"),    QStringLiteral(".cab"),
        QStringLiteral(".zst"),   QStringLiteral(".xz"),     QStringLiteral(".bz2"),
        QStringLiteral(".gz"),
    };

    for (const QString &suffix : suffixes) {
        if (lower.endsWith(suffix))
            return true;
    }
    return false;
}

QString VfsUri::archiveUriFor(const QString &fileUri)
{
    const VfsUri uri = VfsUri::parse(fileUri);

    VfsUri archive;
    archive.scheme = QStringLiteral("archive");
    archive.path = uri.isLocal() ? uri.path : uri.toLocalPath();
    return archive.toString();
}

QString vfsLeafName(const QString &uriText)
{
    const VfsUri uri = VfsUri::parse(uriText);

    const QString tail = uri.isArchive() ? uri.inner : uri.path;
    if (tail.isEmpty() || tail == QLatin1String("/"))
        return uri.isArchive() ? QFileInfo(uri.path).fileName() : QStringLiteral("/");

    QString trimmed = tail;
    while (trimmed.endsWith(QLatin1Char('/')))
        trimmed.chop(1);

    const int slash = trimmed.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? trimmed : trimmed.mid(slash + 1);
}

Vfs &Vfs::instance()
{
    static Vfs vfs;
    return vfs;
}

void Vfs::addBackend(std::shared_ptr<VfsBackend> backend)
{
    if (backend)
        m_backends.insert(backend->scheme(), backend);
}

VfsBackend *Vfs::backendFor(const QString &scheme) const
{
    const auto it = m_backends.constFind(scheme);
    return it == m_backends.constEnd() ? nullptr : it.value().get();
}

bool Vfs::list(const QString &uri, bool includeHidden, QVector<Entry> *entries, QString *error)
{
    const VfsUri parsed = VfsUri::parse(uri);
    VfsBackend *backend = backendFor(parsed.scheme);
    if (!backend) {
        if (error)
            *error = QStringLiteral("Unknown protocol: %1").arg(parsed.scheme);
        return false;
    }

    return backend->list(parsed, includeHidden, entries, error);
}

bool Vfs::fetch(const QString &uri, const QString &targetFile, QString *error)
{
    const VfsUri parsed = VfsUri::parse(uri);

    if (parsed.scheme == QLatin1String("file")) {
        if (QFile::exists(targetFile))
            QFile::remove(targetFile);
        if (!QFile::copy(parsed.path, targetFile)) {
            if (error)
                *error = QStringLiteral("Could not copy %1").arg(parsed.path);
            return false;
        }
        return true;
    }

    VfsBackend *backend = backendFor(parsed.scheme);
    if (!backend) {
        if (error)
            *error = QStringLiteral("Unknown protocol: %1").arg(parsed.scheme);
        return false;
    }

    return backend->fetch(parsed, targetFile, error);
}

bool Vfs::writable(const QString &uri) const
{
    const VfsUri parsed = VfsUri::parse(uri);
    const VfsBackend *backend = backendFor(parsed.scheme);
    return backend && backend->writable();
}

bool Vfs::mkdir(const QString &parentUri, const QString &name, QString *error)
{
    const VfsUri parsed = VfsUri::parse(parentUri);
    VfsBackend *backend = backendFor(parsed.scheme);
    if (!backend)
        return false;
    return backend->mkdir(parsed, name, error);
}

bool Vfs::remove(const QString &uri, bool recursive, QString *error)
{
    const VfsUri parsed = VfsUri::parse(uri);
    VfsBackend *backend = backendFor(parsed.scheme);
    if (!backend)
        return false;
    return backend->remove(parsed, recursive, error);
}

bool Vfs::rename(const QString &uri, const QString &newName, QString *newUri, QString *error)
{
    const VfsUri parsed = VfsUri::parse(uri);
    VfsBackend *backend = backendFor(parsed.scheme);
    if (!backend)
        return false;
    return backend->rename(parsed, newName, newUri, error);
}

bool Vfs::put(const QString &targetUri, const QString &name, const QString &localFile, QString *error)
{
    const VfsUri parsed = VfsUri::parse(targetUri);
    VfsBackend *backend = backendFor(parsed.scheme);
    if (!backend)
        return false;
    return backend->put(parsed, name, localFile, error);
}

QStringList Vfs::schemes() const
{
    return m_backends.keys();
}

QString Vfs::localPathOf(const QString &uri)
{
    return VfsUri::parse(uri).toLocalPath();
}

bool Vfs::isLocal(const QString &uri)
{
    return VfsUri::parse(uri).isLocal();
}
