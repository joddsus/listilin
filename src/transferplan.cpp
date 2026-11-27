#include "transferplan.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QObject>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace {

bool sameInode(const QString &a, const QString &b)
{
#ifdef Q_OS_UNIX
    struct ::stat first;
    struct ::stat second;
    if (::lstat(QFile::encodeName(a).constData(), &first) != 0)
        return false;
    if (::lstat(QFile::encodeName(b).constData(), &second) != 0)
        return false;
    return first.st_dev == second.st_dev && first.st_ino == second.st_ino;
#else
    Q_UNUSED(a)
    Q_UNUSED(b)
    return false;
#endif
}

} // namespace

namespace TransferPlan {

QString normalised(const QString &path)
{
    if (path.isEmpty())
        return QString();
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool samePath(const QString &a, const QString &b)
{
    const QString first = normalised(a);
    const QString second = normalised(b);
    if (first.isEmpty() || second.isEmpty())
        return false;
    if (first == second)
        return true;

    // Different routes to one folder through a symlinked parent.
    // A symlinked parent is a different route to the same folder, even when the
    // file itself does not exist yet.
    const QFileInfo firstInfo(first);
    const QFileInfo secondInfo(second);
    if (!firstInfo.isSymLink() && !secondInfo.isSymLink()) {
        const QFileInfo firstParent(firstInfo.absolutePath());
        const QFileInfo secondParent(secondInfo.absolutePath());
        if (firstParent.exists() && secondParent.exists()) {
            const QString firstParentCanonical = firstParent.canonicalFilePath();
            const QString secondParentCanonical = secondParent.canonicalFilePath();
            if (!firstParentCanonical.isEmpty()
                && firstParentCanonical == secondParentCanonical
                && firstInfo.fileName() == secondInfo.fileName()) {
                return true;
            }
        }
    }
    if (firstInfo.exists() && secondInfo.exists() && !firstInfo.isSymLink()
        && !secondInfo.isSymLink()) {
        const QString firstCanonical = firstInfo.canonicalFilePath();
        const QString secondCanonical = secondInfo.canonicalFilePath();
        if (!firstCanonical.isEmpty() && firstCanonical == secondCanonical)
            return true;
        // Two hard links to the same contents are the same object for the
        // purpose of "do not overwrite the file with itself".
        return sameInode(first, second);
    }
    return false;
}

bool isInside(const QString &child, const QString &parent)
{
    const QString childPath = normalised(child);
    const QString parentPath = normalised(parent);
    if (childPath.isEmpty() || parentPath.isEmpty())
        return false;
    if (childPath == parentPath)
        return true;
    return childPath.startsWith(parentPath.endsWith(QLatin1Char('/'))
                                    ? parentPath
                                    : parentPath + QLatin1Char('/'));
}

QString destinationFor(const QString &source, const QString &targetDir)
{
    const QString name = QFileInfo(source).fileName();
    if (name.isEmpty())
        return QString();
    return QDir(targetDir).filePath(name);
}

QString problem(const QString &source, const QString &targetDir, bool moveFiles)
{
    const QFileInfo info(source);
    const QString target = destinationFor(source, targetDir);
    if (target.isEmpty()) {
        return QObject::tr("%1 has no usable name.").arg(source);
    }

    // Moving a file onto itself is a no-op, not a copy followed by a delete of
    // the file that was just published.
    if (samePath(source, target))
        return QString();

    if (info.isDir() && !info.isSymLink()) {
        if (isInside(targetDir, source)) {
            return moveFiles
                ? QObject::tr("Cannot move the folder \"%1\" into itself.").arg(info.fileName())
                : QObject::tr("Cannot copy the folder \"%1\" into itself.").arg(info.fileName());
        }
    }

    return QString();
}

QStringList validate(const QStringList &sources, const QString &targetDir, bool moveFiles)
{
    QStringList problems;
    QHash<QString, QString> destinations;
    QStringList targets;

    for (const QString &source : sources) {
        const QString issue = problem(source, targetDir, moveFiles);
        if (!issue.isEmpty()) {
            problems.append(issue);
            continue;
        }

        const QString target = destinationFor(source, targetDir);
        if (target.isEmpty())
            continue;

        const QString key = normalised(target);
        const auto existing = destinations.constFind(key);
        if (existing != destinations.constEnd()) {
            problems.append(QObject::tr("\"%1\" and \"%2\" would both be copied to \"%3\".")
                                .arg(QFileInfo(existing.value()).fileName(),
                                     QFileInfo(source).fileName(), QFileInfo(target).fileName()));
            continue;
        }
        destinations.insert(key, source);
        targets.append(target);
    }

    if (!problems.isEmpty())
        return problems;

    // The destination of one entry must never be the source of another: that
    // would overwrite a file from the same job before it had been copied.
    for (const QString &target : targets) {
        for (const QString &source : sources) {
            if (samePath(target, source) && !samePath(source, destinationFor(source, targetDir)))
                problems.append(QObject::tr("The destination \"%1\" would replace another selected item.")
                                    .arg(QFileInfo(target).fileName()));
        }
    }

    return problems;
}

} // namespace TransferPlan
