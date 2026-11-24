#pragma once

#include <QByteArray>
#include <QString>

// Streaming file hashing helpers, kept free of widgets so they can be tested
// without a GUI. Used by the "Copy checksums" command and duplicate detection.
namespace FileHash {

enum class Algorithm {
    Md5,
    Sha1,
    Sha256,
};

QString algorithmName(Algorithm algorithm);
QStringList supportedAlgorithms();

// Computes the digest of `path` using `algorithm`. Returns false on I/O error.
bool hashFile(const QString &path, Algorithm algorithm, QByteArray *digest,
              QString *error = nullptr);

// Convenience wrapper returning the lowercase hex digest, or an empty string on
// failure (with `error` populated when provided).
QString hexDigest(const QString &path, Algorithm algorithm, QString *error = nullptr);

} // namespace FileHash