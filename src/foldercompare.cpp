#include "foldercompare.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace {

struct Node
{
    bool isDir = false;
    bool isSymLink = false;
    bool isFile = false;
    qint64 size = 0;
    QString linkTarget;
};

bool collect(const QString &root, bool includeHidden, const QStringList &extensions,
             QHash<QString, Node> *nodes, FolderCompare::Result *result,
             const std::atomic_bool *cancel)
{
    QDir base(root);
    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (includeHidden)
        filters |= QDir::Hidden;

    QDirIterator it(root, filters, QDirIterator::Subdirectories);
    int seen = 0;
    while (it.hasNext()) {
        if ((++seen & 0xff) == 0 && cancel && cancel->load()) {
            result->cancelled = true;
            return false;
        }

        it.next();
        const QFileInfo info = it.fileInfo();
        Node node;
        node.isSymLink = info.isSymLink();
        node.isDir = info.isDir() && !node.isSymLink;
        node.isFile = info.isFile() && !node.isSymLink;
        node.size = node.isFile ? info.size() : 0;
        if (node.isSymLink)
            node.linkTarget = info.symLinkTarget();

        // Extension filter: skip files whose suffix is not in the list. Keep
        // directories so the comparison still sees the tree shape.
        if (node.isFile && !node.isSymLink && !extensions.isEmpty()) {
            const QString suffix = info.suffix().toLower();
            bool match = false;
            for (const QString &ext : extensions) {
                if (ext.trimmed().toLower().remove(QChar('.')) == suffix) {
                    match = true;
                    break;
                }
            }
            if (!match)
                continue;
        }

        nodes->insert(base.relativeFilePath(info.absoluteFilePath()), node);
    }
    return true;
}

bool hashFile(const QString &path, QByteArray *digest, qint64 *bytes, QString *error,
              const std::atomic_bool *cancel, bool *cancelled)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buffer(1024 * 1024, Qt::Uninitialized);
    while (!file.atEnd()) {
        if (cancel && cancel->load()) {
            *cancelled = true;
            return false;
        }

        const qint64 read = file.read(buffer.data(), buffer.size());
        if (read < 0) {
            *error = file.errorString();
            return false;
        }
        if (read == 0)
            break;

        hash.addData(QByteArrayView(buffer.constData(), static_cast<qsizetype>(read)));
        *bytes += read;
    }

    *digest = hash.result();
    return true;
}

bool hasAncestor(const QString &path, const QSet<QString> &directories)
{
    int slash = path.indexOf(QLatin1Char('/'));
    while (slash >= 0) {
        if (directories.contains(path.left(slash)))
            return true;
        slash = path.indexOf(QLatin1Char('/'), slash + 1);
    }
    return false;
}

} // namespace

namespace FolderCompare {

Result run(const QString &leftRoot, const QString &rightRoot, const Options &options,
           const std::atomic_bool *cancel)
{
    Result result;

    const QFileInfo leftInfo(leftRoot);
    const QFileInfo rightInfo(rightRoot);
    if (!leftInfo.exists() || !leftInfo.isDir()) {
        result.error = QObject::tr("Left folder does not exist: %1").arg(leftRoot);
        return result;
    }
    if (!rightInfo.exists() || !rightInfo.isDir()) {
        result.error = QObject::tr("Right folder does not exist: %1").arg(rightRoot);
        return result;
    }
    if (!QDir(leftRoot).isReadable() || !QDir(rightRoot).isReadable()) {
        result.error = QObject::tr("Both folders must be readable to compare them.");
        return result;
    }

    // Comparing a folder with itself is a no-op, not a reason to walk it twice.
    if (!leftInfo.canonicalFilePath().isEmpty()
        && leftInfo.canonicalFilePath() == rightInfo.canonicalFilePath()) {
        return result;
    }

    QHash<QString, Node> left;
    QHash<QString, Node> right;
    if (!collect(leftRoot, options.includeHidden, options.extensions, &left, &result, cancel)
        || !collect(rightRoot, options.includeHidden, options.extensions, &right, &result, cancel)) {
        return result;
    }

    QStringList paths = left.keys();
    const QStringList rightKeys = right.keys();
    for (const QString &path : rightKeys) {
        if (!left.contains(path))
            paths.append(path);
    }
    std::sort(paths.begin(), paths.end(), [](const QString &a, const QString &b) {
        return a < b;
    });

    QSet<QString> leftOnlyDirs;
    QSet<QString> rightOnlyDirs;

    for (const QString &path : paths) {
        if (cancel && cancel->load()) {
            result.cancelled = true;
            break;
        }

        const bool inLeft = left.contains(path);
        const bool inRight = right.contains(path);

        if (!inRight) {
            const Node node = left.value(path);
            if (!hasAncestor(path, leftOnlyDirs)) {
                result.items.append({Difference::OnlyLeft, path, node.size, -1, QString()});
                if (node.isDir)
                    leftOnlyDirs.insert(path);
            }
            continue;
        }

        if (!inLeft) {
            const Node node = right.value(path);
            if (!hasAncestor(path, rightOnlyDirs)) {
                result.items.append({Difference::OnlyRight, path, -1, node.size, QString()});
                if (node.isDir)
                    rightOnlyDirs.insert(path);
            }
            continue;
        }

        const Node l = left.value(path);
        const Node r = right.value(path);

        if (l.isDir || r.isDir) {
            if (l.isDir && r.isDir) {
                ++result.directoriesCompared;
            } else {
                result.items.append({Difference::TypeChanged, path, l.size, r.size, QString()});
            }
            continue;
        }

        if (l.isSymLink || r.isSymLink) {
            if (!l.isSymLink || !r.isSymLink) {
                result.items.append({Difference::TypeChanged, path, l.size, r.size, QString()});
            } else if (l.linkTarget != r.linkTarget) {
                result.items.append({Difference::LinkChanged, path, 0, 0,
                                     QStringLiteral("%1 → %2").arg(l.linkTarget, r.linkTarget)});
            }
            continue;
        }

        if (l.isFile != r.isFile) {
            result.items.append({Difference::TypeChanged, path, l.size, r.size, QString()});
            continue;
        }

        ++result.filesCompared;

        if (l.size != r.size) {
            result.items.append({Difference::SizeChanged, path, l.size, r.size, QString()});
            continue;
        }

        if (!options.compareContents || !l.isFile)
            continue;

        QByteArray leftHash;
        QByteArray rightHash;
        QString leftError;
        QString rightError;
        bool cancelled = false;
        qint64 leftBytes = 0;
        qint64 rightBytes = 0;

        if (!hashFile(QDir(leftRoot).filePath(path), &leftHash, &leftBytes, &leftError, cancel,
                      &cancelled)) {
            if (cancelled) {
                result.cancelled = true;
                break;
            }
            result.items.append({Difference::ReadError, path, l.size, r.size,
                                 QObject::tr("left: %1").arg(leftError)});
            continue;
        }

        if (!hashFile(QDir(rightRoot).filePath(path), &rightHash, &rightBytes, &rightError, cancel,
                      &cancelled)) {
            if (cancelled) {
                result.cancelled = true;
                break;
            }
            result.items.append({Difference::ReadError, path, l.size, r.size,
                                 QObject::tr("right: %1").arg(rightError)});
            continue;
        }

        result.bytesHashed += leftBytes + rightBytes;
        if (leftHash != rightHash)
            result.items.append({Difference::ContentChanged, path, l.size, r.size, QString()});
    }

    std::sort(result.items.begin(), result.items.end(), [](const Item &a, const Item &b) {
        return a.relativePath < b.relativePath;
    });
    return result;
}

} // namespace FolderCompare
