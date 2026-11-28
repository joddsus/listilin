#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

#include <atomic>

namespace FolderCompare {

struct Options
{
    bool includeHidden = true;
    bool compareContents = false;
    // When non-empty, only files whose suffix (case-insensitive, without the
    // dot) is in this list are compared. Directories are always walked so the
    // tree shape stays intact; only file entries are filtered out.
    QStringList extensions;
};

enum class Difference {
    OnlyLeft,
    OnlyRight,
    TypeChanged,
    SizeChanged,
    ContentChanged,
    LinkChanged,
    ReadError,
};

struct Item
{
    Difference kind = Difference::OnlyLeft;
    QString relativePath;
    qint64 leftSize = -1;
    qint64 rightSize = -1;
    QString detail;
};

struct Result
{
    QVector<Item> items;
    QString error;
    bool cancelled = false;
    // True when part of the tree could not be listed or read. The comparison is
    // then incomplete: an empty item list must never be reported as "match".
    bool incomplete = false;
    // One line per folder or file that could not be read, naming its side.
    QStringList unreadable;
    int filesCompared = 0;
    int directoriesCompared = 0;
    qint64 bytesHashed = 0;
};

// Compares two local directory trees recursively. Content checks use SHA-256
// only for files whose sizes already match, so the common case stays cheap.
// The cancel flag is optional and may be set from another thread.
Result run(const QString &leftRoot, const QString &rightRoot, const Options &options,
           const std::atomic_bool *cancel = nullptr);

} // namespace FolderCompare
