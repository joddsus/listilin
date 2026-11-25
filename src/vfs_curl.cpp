#include "pathsafety.h"
#include "vfs_curl.h"

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QStringList>

#include <curl/curl.h>

namespace {

// FTP/SFTP commands are sent one per QUOTE entry, but an embedded CRLF in a
// path would still be read as a second command by the server. Reject any
// control character before it can leave the process.
bool hasControlChars(const QString &text)
{
    for (const QChar &ch : text) {
        if (ch == QLatin1Char('\r') || ch == QLatin1Char('\n') || ch.unicode() < 0x20)
            return true;
    }
    return false;
}

struct CurlGlobal
{
    CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};

void ensureCurl()
{
    static CurlGlobal global;
    Q_UNUSED(global)
}

// libcurl reads ftps:// as implicit TLS and has no scheme of its own for the
// explicit (AUTH TLS) flavour, which is what most servers offer. ftpes:// is
// spoken as plain FTP with a request to upgrade both channels.
QString transportScheme(const QString &scheme)
{
    return scheme == QLatin1String("ftpes") ? QStringLiteral("ftp") : scheme;
}

void applyTls(CURL *handle, const QString &scheme)
{
    if (scheme == QLatin1String("ftpes"))
        curl_easy_setopt(handle, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));
}

QString urlFor(const VfsUri &uri)
{
    QString path = uri.path;
    if (path.isEmpty())
        path = QStringLiteral("/");
    if (!path.endsWith(QLatin1Char('/')))
        path += QLatin1Char('/');

    return transportScheme(uri.scheme) + QStringLiteral("://") + uri.authority + path;
}

size_t collect(char *data, size_t size, size_t nmemb, void *userdata)
{
    auto *buffer = static_cast<QByteArray *>(userdata);
    buffer->append(data, static_cast<int>(size * nmemb));
    return size * nmemb;
}

size_t writeToFile(char *data, size_t size, size_t nmemb, void *userdata)
{
    auto *file = static_cast<QFile *>(userdata);
    const qint64 written = file->write(data, static_cast<qint64>(size * nmemb));
    return written < 0 ? 0 : static_cast<size_t>(written);
}

// FTP servers answer LIST in whatever dialect they like; this handles the two
// common shapes (Unix long format and plain names) and gives up gracefully on
// anything else by treating the line as a name.
Entry parseListingLine(const QString &line)
{
    Entry entry;
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return entry;

    if (trimmed.startsWith(QLatin1String("total ")))
        return entry;

    const QStringList parts = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() >= 9 && parts.first().size() >= 10
        && (parts.first().startsWith(QLatin1Char('d')) || parts.first().startsWith(QLatin1Char('-'))
            || parts.first().startsWith(QLatin1Char('l')))) {
        entry.isDir = parts.first().startsWith(QLatin1Char('d'));

        bool ok = false;
        const qint64 size = parts.at(4).toLongLong(&ok);
        entry.size = ok && !entry.isDir ? size : 0;

        QString name = parts.mid(8).join(QLatin1Char(' '));
        const int arrow = name.indexOf(QLatin1String(" -> "));
        if (arrow > 0)
            name = name.left(arrow);
        entry.name = name;

        static const QStringList months = {
            QStringLiteral("Jan"), QStringLiteral("Feb"), QStringLiteral("Mar"),
            QStringLiteral("Apr"), QStringLiteral("May"), QStringLiteral("Jun"),
            QStringLiteral("Jul"), QStringLiteral("Aug"), QStringLiteral("Sep"),
            QStringLiteral("Oct"), QStringLiteral("Nov"), QStringLiteral("Dec"),
        };
        // Two common orderings turn up in the wild: "Nov 17 01:25" (Unix ls)
        // and "17 Nov 01:25" (libcurl's SFTP listing). Accept either.
        int dayIndex = 6;
        int timeIndex = 7;
        int month = months.indexOf(parts.at(5)) + 1;
        if (month == 0 && parts.size() >= 9) {
            month = months.indexOf(parts.at(6)) + 1;
            if (month > 0) {
                dayIndex = 5;
                timeIndex = 7;
            }
        }
        if (month > 0) {
            const int day = parts.at(dayIndex).toInt();
            const QString timePart = parts.at(timeIndex);
            QDate date(QDate::currentDate().year(), month, day);
            if (timePart.contains(QLatin1Char(':'))) {
                const QTime time = QTime::fromString(timePart, QStringLiteral("HH:mm"));
                entry.modified = QDateTime(date, time.isValid() ? time : QTime(0, 0));
            } else {
                const int year = timePart.toInt();
                if (year > 1900)
                    date.setDate(year, month, day);
                entry.modified = QDateTime(date, QTime(0, 0));
            }
        }

        return entry;
    }

    entry.name = trimmed;
    return entry;
}

} // namespace

CurlBackend::CurlBackend(QString scheme)
    : m_scheme(std::move(scheme))
{
}

bool CurlBackend::list(const VfsUri &uri, bool includeHidden, QVector<Entry> *entries,
                       QString *error)
{
    Q_UNUSED(includeHidden)
    ensureCurl();

    CURL *handle = curl_easy_init();
    if (!handle) {
        if (error)
            *error = QStringLiteral("Could not start the network client");
        return false;
    }

    QByteArray listing;
    const QByteArray url = urlFor(uri).toUtf8();

    curl_easy_setopt(handle, CURLOPT_URL, url.constData());
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &listing);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "Listista/0.1");
    curl_easy_setopt(handle, CURLOPT_FILETIME, 1L);
    applyTls(handle, m_scheme);

    const CURLcode result = curl_easy_perform(handle);
    if (result != CURLE_OK) {
        if (error)
            *error = QString::fromUtf8(curl_easy_strerror(result));
        curl_easy_cleanup(handle);
        return false;
    }

    curl_easy_cleanup(handle);

    entries->clear();
    const QStringList lines = QString::fromUtf8(listing).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    entries->reserve(lines.size());

    for (const QString &line : lines) {
        const Entry entry = parseListingLine(line);
        if (entry.name.isEmpty() || entry.name == QLatin1String(".") || entry.name == QLatin1String("..")
            || !PathSafety::isSafePathComponent(entry.name))
            continue;
        entries->append(entry);
    }

    if (error)
        error->clear();
    return true;
}

bool CurlBackend::fetch(const VfsUri &uri, const QString &targetFile, QString *error)
{
    ensureCurl();

    CURL *handle = curl_easy_init();
    if (!handle) {
        if (error)
            *error = QStringLiteral("Could not start the network client");
        return false;
    }

    QFile out(targetFile);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("Could not write %1").arg(targetFile);
        curl_easy_cleanup(handle);
        return false;
    }

    const QString remote = transportScheme(uri.scheme) + QStringLiteral("://") + uri.authority
                           + uri.path;
    const QByteArray url = remote.toUtf8();

    curl_easy_setopt(handle, CURLOPT_URL, url.constData());
    applyTls(handle, m_scheme);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeToFile);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "Listista/0.1");

    const CURLcode result = curl_easy_perform(handle);
    out.close();
    curl_easy_cleanup(handle);

    if (result != CURLE_OK) {
        if (error)
            *error = QString::fromUtf8(curl_easy_strerror(result));
        QFile::remove(targetFile);
        return false;
    }

    if (error)
        error->clear();
    return true;
}

bool CurlBackend::mkdir(const VfsUri &parent, const QString &name, QString *error)
{
    if (hasControlChars(parent.path) || hasControlChars(name)) {
        if (error)
            *error = QStringLiteral("Invalid characters in the path.");
        return false;
    }
    ensureCurl();
    const QString url = urlFor(parent);

    auto runQuote = [&](const QString &command) -> bool {
        CURL *handle = curl_easy_init();
        if (!handle) {
            if (error)
                *error = QStringLiteral("Could not start the network client");
            return false;
        }
        struct curl_slist *quotes = nullptr;
        const QByteArray cmd = command.toUtf8();
        quotes = curl_slist_append(quotes, cmd.constData());
        curl_easy_setopt(handle, CURLOPT_URL, url.toUtf8().constData());
        curl_easy_setopt(handle, CURLOPT_QUOTE, quotes);
        curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 60L);
        curl_easy_setopt(handle, CURLOPT_USERAGENT, "Listista/0.1");
        applyTls(handle, m_scheme);

        const CURLcode result = curl_easy_perform(handle);
        curl_slist_free_all(quotes);
        curl_easy_cleanup(handle);

        if (result != CURLE_OK) {
            if (error)
                *error = QString::fromUtf8(curl_easy_strerror(result));
            return false;
        }
        if (error)
            error->clear();
        return true;
    };

    QString full = parent.path;
    while (full.endsWith(QLatin1Char('/')))
        full.chop(1);
    full += QLatin1Char('/') + name;
    const QString command = m_scheme == QLatin1String("sftp")
                                ? QStringLiteral("mkdir ") + full
                                : QStringLiteral("MKD ") + full;
    return runQuote(command);
}

bool CurlBackend::remove(const VfsUri &uri, bool recursive, QString *error)
{
    if (hasControlChars(uri.path)) {
        if (error)
            *error = QStringLiteral("Invalid characters in the path.");
        return false;
    }
    const QString parentUrl = urlFor(VfsUri::parse(VfsUri::parentUri(uri.toString())));

    if (recursive) {
        // Best effort: list the target if it is a directory and remove its
        // children first. We only know it is a directory if listing succeeds
        // and returns entries; otherwise treat it as a file. A trailing slash
        // on the URI must not skip this, or a non-empty directory fails RMD.
        QVector<Entry> entries;
        QString listError;
        // uri.path may be a file path; listing a file fails, which is fine.
        if (Vfs::instance().list(uri.toString(), true, &entries, &listError)) {
            for (const Entry &entry : entries) {
                const QString childUri = VfsUri::childUri(uri.toString(), entry.name);
                if (!remove(VfsUri::parse(childUri), true, error))
                    return false;
            }
        }
    }

    auto runQuote = [&](const QString &command) -> bool {
        CURL *handle = curl_easy_init();
        if (!handle) {
            if (error)
                *error = QStringLiteral("Could not start the network client");
            return false;
        }
        struct curl_slist *quotes = nullptr;
        const QByteArray cmd = command.toUtf8();
        quotes = curl_slist_append(quotes, cmd.constData());
        curl_easy_setopt(handle, CURLOPT_URL, parentUrl.toUtf8().constData());
        curl_easy_setopt(handle, CURLOPT_QUOTE, quotes);
        curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 60L);
        curl_easy_setopt(handle, CURLOPT_USERAGENT, "Listista/0.1");
        applyTls(handle, m_scheme);

        const CURLcode result = curl_easy_perform(handle);
        curl_slist_free_all(quotes);
        curl_easy_cleanup(handle);

        if (result != CURLE_OK) {
            if (error)
                *error = QString::fromUtf8(curl_easy_strerror(result));
            return false;
        }
        if (error)
            error->clear();
        return true;
    };

    // Try directory removal first, then file removal.
    const QString full = uri.path;
    const QString rmd = m_scheme == QLatin1String("sftp")
                            ? QStringLiteral("rmdir ") + full
                            : QStringLiteral("RMD ") + full;
    if (runQuote(rmd))
        return true;

    const QString del = m_scheme == QLatin1String("sftp")
                            ? QStringLiteral("rm ") + full
                            : QStringLiteral("DELE ") + full;
    return runQuote(del);
}

bool CurlBackend::rename(const VfsUri &uri, const QString &newName, QString *newUri, QString *error)
{
    if (hasControlChars(uri.path) || hasControlChars(newName)) {
        if (error)
            *error = QStringLiteral("Invalid characters in the path.");
        return false;
    }
    ensureCurl();
    const QString parentUrl = urlFor(VfsUri::parse(VfsUri::parentUri(uri.toString())));
    const QString leaf = vfsLeafName(uri.toString());

    CURL *handle = curl_easy_init();
    if (!handle) {
        if (error)
            *error = QStringLiteral("Could not start the network client");
        return false;
    }

    QString oldFull = uri.path;
    QString newFull = oldFull;
    const int lastSlash = newFull.lastIndexOf(QLatin1Char('/'));
    if (lastSlash >= 0)
        newFull = newFull.left(lastSlash + 1) + newName;
    else
        newFull = QLatin1Char('/') + newName;

    struct curl_slist *quotes = nullptr;
    const QString fromCmd = m_scheme == QLatin1String("sftp")
                                ? QStringLiteral("rename ") + oldFull + QLatin1Char(' ') + newFull
                                : QStringLiteral("RNFR ") + oldFull;
    quotes = curl_slist_append(quotes, fromCmd.toUtf8().constData());
    if (m_scheme != QLatin1String("sftp")) {
        const QByteArray toCmd = (QStringLiteral("RNTO ") + newFull).toUtf8();
        quotes = curl_slist_append(quotes, toCmd.constData());
    }

    curl_easy_setopt(handle, CURLOPT_URL, parentUrl.toUtf8().constData());
    curl_easy_setopt(handle, CURLOPT_QUOTE, quotes);
    curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "Listista/0.1");
    applyTls(handle, m_scheme);

    const CURLcode result = curl_easy_perform(handle);
    curl_slist_free_all(quotes);
    curl_easy_cleanup(handle);

    if (result != CURLE_OK) {
        if (error)
            *error = QString::fromUtf8(curl_easy_strerror(result));
        return false;
    }

    if (newUri) {
        VfsUri u = uri;
        // replace last component
        int slash = u.path.lastIndexOf(QLatin1Char('/'));
        if (slash >= 0)
            u.path = u.path.left(slash + 1) + newName;
        else
            u.path = QLatin1Char('/') + newName;
        *newUri = u.toString();
    }
    if (error)
        error->clear();
    return true;
}

bool CurlBackend::put(const VfsUri &targetDir, const QString &name, const QString &localFile, QString *error)
{
    ensureCurl();
    FILE *fp = nullptr;
#ifdef _WIN32
    _wfopen_s(&fp, reinterpret_cast<const wchar_t *>(localFile.utf16()), L"rb");
#else
    fp = fopen(localFile.toUtf8().constData(), "rb");
#endif
    if (!fp) {
        if (error)
            *error = QStringLiteral("Could not read %1").arg(localFile);
        return false;
    }

    fseek(fp, 0, SEEK_END);
    const long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    CURL *handle = curl_easy_init();
    if (!handle) {
        fclose(fp);
        if (error)
            *error = QStringLiteral("Could not start the network client");
        return false;
    }

    const QString url = urlFor(targetDir) + name;
    curl_easy_setopt(handle, CURLOPT_URL, url.toUtf8().constData());
    applyTls(handle, m_scheme);
    curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(handle, CURLOPT_READDATA, fp);
    if (size >= 0)
        curl_easy_setopt(handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t)size);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "Listista/0.1");

    const CURLcode result = curl_easy_perform(handle);
    curl_easy_cleanup(handle);
    fclose(fp);

    if (result != CURLE_OK) {
        if (error)
            *error = QString::fromUtf8(curl_easy_strerror(result));
        return false;
    }
    if (error)
        error->clear();
    return true;
}
