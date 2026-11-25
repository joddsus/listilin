#pragma once

#include <QString>
#include <QVector>

#include "entry.h"

// Recursive local scan used by Flat View. Entry names are paths relative to the
// scanned root so the same listing can sort by subfolder and open real files.
namespace FlatScan {

QVector<Entry> scanLocal(const QString &root, bool showHidden, QString *error = nullptr,
                         int maxEntries = 20000);

} // namespace FlatScan
