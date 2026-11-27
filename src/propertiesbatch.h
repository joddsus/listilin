#pragma once

#include <QFile>
#include <QString>
#include <QStringList>

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

// Applies `change` to every path, and with `recursive` to every entry inside a
// selected folder. Symbolic links are skipped rather than followed, so a link
// can never change the permissions of the file it points at. Returns one
// human-readable line per entry that could not be changed; empty on success.
QStringList apply(const QStringList &localPaths, const Change &change, bool recursive);

} // namespace PropertiesBatch
