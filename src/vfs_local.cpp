#include "vfs_local.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

bool LocalBackend::list(const VfsUri &uri, bool includeHidden, QVector<Entry> *entries,
                        QString *error)
{
    const QDir dir(uri.path);
    if (!dir.exists()) {
        if (error)
            *error = QStringLiteral("%1 does not exist").arg(uri.path);
        return false;
    }

    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (includeHidden)
        filters |= QDir::Hidden;

    const QFileInfoList infos = dir.entryInfoList(filters, QDir::NoSort);
    entries->clear();
    entries->reserve(infos.size());

    for (const QFileInfo &info : infos) {
        const QString name = info.fileName();
        if (name == QLatin1String(".") || name == QLatin1String(".."))
            continue;

        Entry entry;
        entry.name = name;
        entry.isDir = info.isDir();
        entry.size = entry.isDir ? 0 : info.size();
        entry.modified = info.lastModified();
        entries->append(entry);
    }

    return true;
}

bool LocalBackend::fetch(const VfsUri &uri, const QString &targetFile, QString *error)
{
    if (QFile::exists(targetFile) && !QFile::remove(targetFile)) {
        if (error)
            *error = QStringLiteral("Could not overwrite %1").arg(targetFile);
        return false;
    }

    if (!QFile::copy(uri.path, targetFile)) {
        if (error)
            *error = QStringLiteral("Could not read %1").arg(uri.path);
        return false;
    }

    return true;
}

bool LocalBackend::mkdir(const VfsUri &parent, const QString &name, QString *error)
{
    QDir dir(parent.path);
    if (!dir.mkpath(name)) {
        if (error)
            *error = QStringLiteral("Could not create %1").arg(name);
        return false;
    }
    return true;
}

bool LocalBackend::remove(const VfsUri &uri, bool recursive, QString *error)
{
    QFileInfo info(uri.path);
    if (info.isDir()) {
        if (recursive) {
            if (!QDir(uri.path).removeRecursively()) {
                if (error)
                    *error = QStringLiteral("Could not delete %1").arg(info.fileName());
                return false;
            }
            return true;
        }
        if (!QDir().rmdir(uri.path)) {
            if (error)
                *error = QStringLiteral("Could not delete %1").arg(info.fileName());
            return false;
        }
        return true;
    }
    if (!QFile::remove(uri.path)) {
        if (error)
            *error = QStringLiteral("Could not delete %1").arg(info.fileName());
        return false;
    }
    return true;
}

bool LocalBackend::rename(const VfsUri &uri, const QString &newName, QString *newUri, QString *error)
{
    QFileInfo info(uri.path);
    const QString target = QDir(info.absolutePath()).filePath(newName);
    if (target == info.absoluteFilePath()) {
        if (newUri)
            *newUri = uri.toString();
        return true;
    }
    if (QFileInfo::exists(target)) {
        if (error)
            *error = QStringLiteral("%1 already exists").arg(newName);
        return false;
    }
    if (!QFile::rename(uri.path, target)) {
        if (error)
            *error = QStringLiteral("Could not rename %1").arg(info.fileName());
        return false;
    }
    if (newUri) {
        VfsUri u = uri;
        u.path = target;
        *newUri = u.toString();
    }
    return true;
}

bool LocalBackend::put(const VfsUri &targetDir, const QString &name, const QString &localFile, QString *error)
{
    const QString dest = QDir(targetDir.path).filePath(name);
    if (QFile::exists(dest) && !QFile::remove(dest)) {
        if (error)
            *error = QStringLiteral("Could not overwrite %1").arg(name);
        return false;
    }
    if (!QFile::copy(localFile, dest)) {
        if (error)
            *error = QStringLiteral("Could not write %1").arg(name);
        return false;
    }
    return true;
}
