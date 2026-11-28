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
    // True when the backend knows the exact byte size, so a zero size can be
    // told apart from "this server did not report one" before a move deletes
    // the source.
    bool hasSize = false;
    // SHA-256 of the member, when the backend can list it cheaply. Empty means
    // the copy has to be read back to be verified.
    QString digest;
    QDateTime modified;
    bool isDir = false;
};

Q_DECLARE_METATYPE(Entry)
