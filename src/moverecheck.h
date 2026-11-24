#pragma once

#include <QString>

#include <cstdint>

// Snapshot and comparison helpers for the destination of a move operation.
// Listi re-checks the committed destination before it removes the source; this
// module keeps that check free of Qt widgets and filesystem mutation so the
// comparison logic can be tested directly.
namespace MoveRecheck {

struct Stat
{
    bool exists = false;
    qint64 size = 0;
    qint64 mtimeMs = 0;
    quint64 deviceId = 0;
    quint64 inode = 0;
    // SHA-256 digest of a regular file's contents, empty for directories or
    // unreadable paths. Strengthens the destination re-check beyond metadata.
    QString digest;
};

// Read the current state of a path. Returns exists=false when the path does
// not exist or cannot be stated.
Stat statPath(const QString &path);

// True when two snapshots describe the same file state.
bool same(const Stat &a, const Stat &b);

// True when `current` differs from `committed`, i.e. the destination changed
// after it was written and before the source could be removed.
bool changed(const Stat &committed, const Stat &current);

} // namespace MoveRecheck