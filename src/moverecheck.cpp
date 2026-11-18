#include "moverecheck.h"

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

    return result;
}

bool same(const Stat &a, const Stat &b)
{
    return a.exists == b.exists && a.size == b.size && a.mtimeMs == b.mtimeMs
           && a.deviceId == b.deviceId && a.inode == b.inode;
}

bool changed(const Stat &committed, const Stat &current)
{
    return !same(committed, current);
}

} // namespace MoveRecheck