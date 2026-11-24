#include "filehash.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

namespace FileHash {

QString algorithmName(Algorithm algorithm)
{
    switch (algorithm) {
    case Algorithm::Md5:
        return QStringLiteral("MD5");
    case Algorithm::Sha1:
        return QStringLiteral("SHA-1");
    case Algorithm::Sha256:
        return QStringLiteral("SHA-256");
    }
    return QStringLiteral("SHA-256");
}

QStringList supportedAlgorithms()
{
    return {algorithmName(Algorithm::Md5), algorithmName(Algorithm::Sha1),
            algorithmName(Algorithm::Sha256)};
}

QCryptographicHash::Algorithm toQt(Algorithm algorithm)
{
    switch (algorithm) {
    case Algorithm::Md5:
        return QCryptographicHash::Md5;
    case Algorithm::Sha1:
        return QCryptographicHash::Sha1;
    case Algorithm::Sha256:
        return QCryptographicHash::Sha256;
    }
    return QCryptographicHash::Sha256;
}

bool hashFile(const QString &path, Algorithm algorithm, QByteArray *digest, QString *error)
{
    if (digest)
        digest->clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QObject::tr("Could not open %1").arg(path);
        return false;
    }

    QCryptographicHash hash(toQt(algorithm));
    constexpr qint64 chunkSize = 64 * 1024;
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(chunkSize);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            if (error)
                *error = QObject::tr("Could not read %1").arg(path);
            file.close();
            return false;
        }
        hash.addData(chunk);
    }
    file.close();

    if (digest)
        *digest = hash.result();
    return true;
}

QString hexDigest(const QString &path, Algorithm algorithm, QString *error)
{
    QByteArray digest;
    if (!hashFile(path, algorithm, &digest, error))
        return QString();
    return QString::fromLatin1(digest.toHex());
}

} // namespace FileHash