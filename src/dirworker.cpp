#include "dirworker.h"
#include "flatscan.h"
#include "vfs.h"

void DirWorker::scan(const QString &uri, quint64 token, bool showHidden, bool flat)
{
    QVector<Entry> entries;
    QString error;

    if (uri.isEmpty())
        return;

    if (flat) {
        if (!Vfs::isLocal(uri)) {
            emit failed(uri, token, QObject::tr("Flat View works in local folders."));
            return;
        }
        entries = FlatScan::scanLocal(Vfs::localPathOf(uri), showHidden, &error);
        if (!error.isEmpty()) {
            emit failed(uri, token, error);
            return;
        }
        emit scanned(uri, token, entries);
        return;
    }

    if (!Vfs::instance().list(uri, showHidden, &entries, &error)) {
        emit failed(uri, token, error);
        return;
    }

    emit scanned(uri, token, entries);
}
