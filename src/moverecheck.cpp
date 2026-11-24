#include "moverecheck.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace MoveRecheck {

Stat statPath(const QString &path)
{
    Stat result;
    const QFileInfo info(path);
    if (!info.exists())
        return result;

    result.exists = true;
    result.size = info.size();
    const QDateTime modified = info.lastModified();
    result.mtimeMs = modified.isValid() ? modified.toMSecsSinceEpoch() : 0;

#ifdef Q_OS_UNIX
    struct ::stat st;
    if (::stat(path.toLocal8Bit().constData(), &st) == 0) {
        result.deviceId = static_cast<quint64>(st.st_dev);
        result.inode = static_cast<quint64>(st.st_ino);
        result.size = static_cast<qint64>(st.st_size);
        result.mtimeMs = static_cast<qint64>(st.st_mtime) * 1000
                         + static_cast<qint64>(st.st_mtim.tv_nsec / 1000000);
    }
#endif

    // Hash regular files so a destination that was replaced or truncated after
    // the committed snapshot cannot be mistaken for the original.
    if (info.isFile() && info.isReadable()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            QByteArray buffer;
            buffer.resize(64 * 1024);
            while (true) {
                const qint64 read = file.read(buffer.data(), buffer.size());
                if (read < 0) {
                    result.digest.clear();
                    break;
                }
                if (read == 0)
                    break;
                hash.addData(buffer.left(static_cast<int>(read)));
            }
            if (file.error() == QFile::NoError)
                result.digest = QString::fromLatin1(hash.result().toHex());
            file.close();
        }
    }

    return result;
}

bool same(const Stat &a, const Stat &b)
{
    // Digests are only meaningful for regular files. When both are empty (for
    // example directories), metadata alone decides equality.
    const bool digestsComparable = !a.digest.isEmpty() || !b.digest.isEmpty();
    return a.exists == b.exists && a.size == b.size && a.mtimeMs == b.mtimeMs
           && a.deviceId == b.deviceId && a.inode == b.inode
           && (!digestsComparable || a.digest == b.digest);
}

bool changed(const Stat &committed, const Stat &current)
{
    return !same(committed, current);
}

} // namespace MoveRecheck