#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "entry.h"

// Scans a location off the UI thread. One scan at a time; a stale result is
// discarded by the token, so a slow archive or a slow FTP site never blocks the
// window or overwrites a newer listing.
class DirWorker : public QObject
{
    Q_OBJECT

public slots:
    void scan(const QString &uri, quint64 token, bool showHidden, bool flat);

signals:
    void scanned(const QString &uri, quint64 token, const QVector<Entry> &entries);
    void failed(const QString &uri, quint64 token, const QString &error);
};
