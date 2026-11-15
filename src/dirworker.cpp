#include "dirworker.h"
#include "vfs.h"

void DirWorker::scan(const QString &uri, quint64 token, bool showHidden)
{
    QVector<Entry> entries;
    QString error;

    if (uri.isEmpty())
        return;

    if (!Vfs::instance().list(uri, showHidden, &entries, &error)) {
        emit failed(uri, token, error);
        return;
    }

    emit scanned(uri, token, entries);
}
