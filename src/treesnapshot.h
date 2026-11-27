#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// A structural snapshot of a file or folder tree, used to prove that a move
// really delivered everything before the source is removed. Comparing the sum
// of bytes is not enough: an empty file or a symlink that never arrived would
// not change the total.
namespace TreeSnapshot {

enum class Kind
{
    File,
    Dir,
    SymLink,
    Other,
};

struct Item
{
    QString relative; // path below the root; empty for the root itself
    Kind kind = Kind::Other;
    qint64 size = 0;
    QString linkTarget; // for symlinks
    QString digest;     // SHA-256 of regular files, empty otherwise
};

struct Tree
{
    QVector<Item> items;
    bool complete = false;
    QString error; // why the tree could not be read, when complete is false
};

// Reads `root` without following symlinks. `complete` is false when any part of
// the tree could not be read, so a caller never treats an unreadable subtree as
// an empty one.
Tree capture(const QString &root);

// Differences between two snapshots, one readable line each. Empty when the
// trees have the same names, types, sizes, link targets and contents.
QStringList compare(const Tree &expected, const Tree &actual);

// Differences that would leave something from `expected` missing or different
// in `actual`. Extra items in `actual` are ignored, which is what a merge into
// an existing folder needs.
QStringList missing(const Tree &expected, const Tree &actual);

// The text stored in a symlink, exactly as the filesystem holds it. Qt's
// QFileInfo::symLinkTarget() resolves relative links to an absolute path, which
// would rewrite a moved relative link into one pointing back at the old place.
QString readLinkTarget(const QString &path);

// SHA-256 of a single regular file, empty when it cannot be read.
QString digestOf(const QString &path);

// A digest over the whole tree (names, types, sizes, link targets, contents),
// used by undo to notice a destination that changed after the job finished.
QString fingerprint(const QString &path);

} // namespace TreeSnapshot
