#pragma once

#include <QFutureWatcher>
#include <QMetaType>

#include <memory>
#include <QObject>
#include <QString>
#include <QStringList>

#include "searchscan.h"

Q_DECLARE_METATYPE(SearchScan::Result)

// Runs a SearchScan on a pooled thread and reports back on the UI thread, so a
// search over a large tree never blocks painting or input. Cancellation is
// polled by the scan itself.
class SearchTask : public QObject
{
    Q_OBJECT

public:
    static SearchTask *start(const QString &root, const SearchScan::Query &query,
                             QObject *parent = nullptr);
    // Searches an archive or remote folder through the VFS. Returns entry URIs.
    static SearchTask *startVfs(const QString &rootUri, const SearchScan::Query &query,
                                QObject *parent = nullptr);
    ~SearchTask() override;

    void cancel();
    bool isCancelled() const;

signals:
    void progress(int scannedEntries, int matches, const QString &current);
    void finished(const SearchScan::Result &result);

private:
    struct State;
    std::shared_ptr<State> m_state;
    class QTimer *m_timer = nullptr;
    QFutureWatcher<SearchScan::Result> *m_watcher = nullptr;
    QString m_root;

    SearchTask(const QString &root, const SearchScan::Query &query, bool vfs, QObject *parent);
    void poll();
};
