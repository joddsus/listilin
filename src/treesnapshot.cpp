#include "treesnapshot.h"

#include "filehash.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QObject>

#include <algorithm>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace {

using TreeSnapshot::Item;
using TreeSnapshot::Kind;

QString kindName(Kind kind)
{
    switch (kind) {
    case Kind::File:
        return QStringLiteral("file");
    case Kind::Dir:
        return QStringLiteral("folder");
    case Kind::SymLink:
        return QStringLiteral("link");
    case Kind::Other:
        break;
    }
    return QStringLiteral("other");
}

bool captureInto(const QString &path, const QString &relative, QVector<Item> *items,
                 QString *error)
{
    const QFileInfo info(path);

    Item item;
    item.relative = relative;

    if (info.isSymLink()) {
        item.kind = Kind::SymLink;
        item.linkTarget = TreeSnapshot::readLinkTarget(path);
        items->append(item);
        return true;
    }

    if (info.isDir()) {
        item.kind = Kind::Dir;
        items->append(item);

        QDir dir(path);
        if (!dir.isReadable()) {
            *error = QObject::tr("Could not read the folder %1").arg(path);
            return false;
        }

        QFileInfoList children = dir.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
        for (const QFileInfo &child : children) {
            const QString childRelative
                = relative.isEmpty() ? child.fileName() : relative + QLatin1Char('/') + child.fileName();
            if (!captureInto(child.absoluteFilePath(), childRelative, items, error))
                return false;
        }
        return true;
    }

    if (info.isFile()) {
        item.kind = Kind::File;
        item.size = info.size();
        item.digest = FileHash::hexDigest(path, FileHash::Algorithm::Sha256, nullptr);
        if (item.digest.isEmpty()) {
            *error = QObject::tr("Could not read the file %1").arg(path);
            return false;
        }
        items->append(item);
        return true;
    }

    // Sockets, pipes and devices have no contents to compare, but their
    // presence still has to be accounted for.
    item.kind = Kind::Other;
    items->append(item);
    return true;
}

QHash<QString, Item> byRelative(const QVector<Item> &items)
{
    QHash<QString, Item> map;
    for (const Item &item : items)
        map.insert(item.relative, item);
    return map;
}

QString describeDiff(const Item &expected, const Item *actual)
{
    if (!actual) {
        return QObject::tr("%1 is missing").arg(
            expected.relative.isEmpty() ? QStringLiteral("the item") : expected.relative);
    }
    if (expected.kind != actual->kind) {
        return QObject::tr("%1 changed from a %2 to a %3")
            .arg(expected.relative, kindName(expected.kind), kindName(actual->kind));
    }
    if (expected.kind == Kind::SymLink && expected.linkTarget != actual->linkTarget) {
        return QObject::tr("%1 points at %2 instead of %3")
            .arg(expected.relative, actual->linkTarget, expected.linkTarget);
    }
    if (expected.kind == Kind::File
        && (expected.size != actual->size || expected.digest != actual->digest)) {
        return QObject::tr("%1 does not match the source copy").arg(expected.relative);
    }
    return QString();
}

} // namespace

namespace TreeSnapshot {

Tree capture(const QString &root)
{
    Tree tree;
    QString error;
    if (!captureInto(root, QString(), &tree.items, &error)) {
        tree.complete = false;
        tree.error = error;
        return tree;
    }
    tree.complete = true;
    std::sort(tree.items.begin(), tree.items.end(), [](const Item &a, const Item &b) {
        return a.relative < b.relative;
    });
    return tree;
}

QStringList compare(const Tree &expected, const Tree &actual)
{
    QStringList differences;
    if (!expected.complete) {
        differences.append(expected.error.isEmpty()
                               ? QObject::tr("The source tree could not be read")
                               : expected.error);
        return differences;
    }
    if (!actual.complete) {
        differences.append(actual.error.isEmpty()
                               ? QObject::tr("The destination tree could not be read")
                               : actual.error);
        return differences;
    }

    const QHash<QString, Item> actualItems = byRelative(actual.items);
    for (const Item &item : expected.items) {
        const auto found = actualItems.constFind(item.relative);
        const QString difference = describeDiff(item, found == actualItems.constEnd() ? nullptr : &*found);
        if (!difference.isEmpty())
            differences.append(difference);
    }

    const QHash<QString, Item> expectedItems = byRelative(expected.items);
    for (const Item &item : actual.items) {
        if (!expectedItems.contains(item.relative))
            differences.append(QObject::tr("%1 is not in the source").arg(item.relative));
    }

    return differences;
}

QStringList missing(const Tree &expected, const Tree &actual)
{
    QStringList differences;
    if (!expected.complete) {
        differences.append(expected.error.isEmpty()
                               ? QObject::tr("The source tree could not be read")
                               : expected.error);
        return differences;
    }
    if (!actual.complete) {
        differences.append(actual.error.isEmpty()
                               ? QObject::tr("The destination tree could not be read")
                               : actual.error);
        return differences;
    }

    const QHash<QString, Item> actualItems = byRelative(actual.items);
    for (const Item &item : expected.items) {
        const auto found = actualItems.constFind(item.relative);
        const QString difference = describeDiff(item, found == actualItems.constEnd() ? nullptr : &*found);
        if (!difference.isEmpty())
            differences.append(difference);
    }
    return differences;
}

QString readLinkTarget(const QString &path)
{
#ifdef Q_OS_UNIX
    QByteArray buffer;
    buffer.resize(4096);
    const ssize_t length = ::readlink(QFile::encodeName(path).constData(), buffer.data(),
                                      size_t(buffer.size()));
    if (length > 0)
        return QString::fromUtf8(buffer.constData(), int(length));
#endif
    return QFileInfo(path).symLinkTarget();
}

QString digestOf(const QString &path)
{
    return FileHash::hexDigest(path, FileHash::Algorithm::Sha256, nullptr);
}

QString fingerprint(const QString &path)
{
    const Tree tree = capture(path);
    if (!tree.complete)
        return QString();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const Item &item : tree.items) {
        hash.addData(item.relative.toUtf8());
        hash.addData(QByteArray(1, '\0'));
        hash.addData(kindName(item.kind).toUtf8());
        hash.addData(QByteArray(1, '\0'));
        hash.addData(QByteArray::number(item.size));
        hash.addData(QByteArray(1, '\0'));
        hash.addData(item.linkTarget.toUtf8());
        hash.addData(QByteArray(1, '\0'));
        hash.addData(item.digest.toUtf8());
        hash.addData(QByteArray(1, '\n'));
    }
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace TreeSnapshot
