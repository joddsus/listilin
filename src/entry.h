#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>

// One row in a lister. Deliberately a plain value type: the worker thread
// produces these, the model owns them, the delegate only reads them.
struct Entry
{
    QString name;
    qint64 size = 0;
    QDateTime modified;
    bool isDir = false;
};

Q_DECLARE_METATYPE(Entry)
