#pragma once

#include <QFile>
#include <QString>
#include <QStringList>
#include <QVector>

// Helpers for the Properties dialog over several local items: one aggregate
// summary and one permission change applied to every selected item (and, when
// asked, to everything inside selected folders). Pure and widget-free so the
// permission rules can be tested against a temporary tree.
namespace PropertiesBatch {

struct Summary
{
    int files = 0;
    int folders = 0;
    qint64 totalSize = 0;
    // The folder every item lives in, or empty when they differ. For a single
    // item this is its parent folder, matching the single-item dialog.
    QString location;
};

Summary summarize(const QStringList &localPaths);

// A change to permissions expressed as bits to turn on and bits to turn off.
// Bits in neither set keep the value they have on each item, which is what a
// partially checked box in the dialog means.
struct Change
{
    QFile::Permissions set;
    QFile::Permissions clear;

    bool isEmpty() const { return set == QFile::Permissions() && clear == QFile::Permissions(); }
};

// What one permission box showed when the dialog opened, and what the user
// left it at. "Mixed" is the partially-checked state used for a selection with
// different bits; leaving it mixed means "do not touch this bit".
enum class BoxState
{
    Off,
    On,
    Mixed,
};

// Builds the change for only the boxes the user actually changed. Fields that
// still show their initial state are not turned into an explicit set/clear, so
// a recursive change preserves each child's own unchanged bits instead of
// copying the selected folder's mode onto it. `bits` is row-major: owner
// read/write/execute, group read/write/execute, other read/write/execute.
Change changesFromStates(const QVector<BoxState> &before, const QVector<BoxState> &after,
                         const QVector<QFile::Permissions> &bits);

// Applies `change` to every path, and with `recursive` to every entry inside a
// selected folder. Symbolic links are skipped rather than followed, so a link
// can never change the permissions of the file it points at. Returns one
// human-readable line per entry that could not be changed; empty on success.
QStringList apply(const QStringList &localPaths, const Change &change, bool recursive);

} // namespace PropertiesBatch
