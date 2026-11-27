#include "searchtask.h"

#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent>

#include <atomic>
#include <mutex>

struct SearchTask::State
{
    std::atomic<int> scanned{0};
    std::atomic<int> matches{0};
    std::atomic<bool> cancelled{false};

    std::mutex mutex;
    QString current;

    void setCurrent(const QString &path)
    {
        std::lock_guard<std::mutex> lock(mutex);
        current = path;
    }

    QString currentPath()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return current;
    }
};

SearchTask::SearchTask(const QString &root, const SearchScan::Query &query, bool vfs,
                       QObject *parent)
    : QObject(parent)
    , m_root(root)
    , m_state(std::make_shared<State>())
{
    SearchScan::Callbacks callbacks;
    callbacks.cancelled = [state = m_state]() { return state->cancelled.load(); };
    callbacks.current = [state = m_state](const QString &path) {
        state->setCurrent(path);
        state->scanned.fetch_add(1);
    };
    callbacks.matched = [state = m_state](int matches) {
        state->matches.store(matches);
    };

    m_timer = new QTimer(this);
    m_timer->setInterval(150);
    connect(m_timer, &QTimer::timeout, this, &SearchTask::poll);

    m_watcher = new QFutureWatcher<SearchScan::Result>(this);
    connect(m_watcher, &QFutureWatcher<SearchScan::Result>::finished, this, [this]() {
        m_timer->stop();
        SearchScan::Result result = m_watcher->result();
        result.cancelled = result.cancelled || isCancelled();
        emit finished(result);
        deleteLater();
    });

    m_watcher->setFuture(QtConcurrent::run([root, query, callbacks, vfs, state = m_state]() {
        SearchScan::Result result = vfs ? SearchScan::runVfs(root, query, callbacks)
                                        : SearchScan::run(root, query, callbacks);
        state->scanned.store(result.scannedEntries);
        state->matches.store(int(result.matches.size()));
        return result;
    }));

    m_timer->start();
}

SearchTask::~SearchTask()
{
    m_state->cancelled.store(true);
}

SearchTask *SearchTask::start(const QString &root, const SearchScan::Query &query,
                              QObject *parent)
{
    return new SearchTask(root, query, false, parent);
}

SearchTask *SearchTask::startVfs(const QString &rootUri, const SearchScan::Query &query,
                                 QObject *parent)
{
    return new SearchTask(rootUri, query, true, parent);
}

void SearchTask::cancel()
{
    m_state->cancelled.store(true);
}

bool SearchTask::isCancelled() const
{
    return m_state->cancelled.load();
}

void SearchTask::poll()
{
    emit progress(m_state->scanned.load(), m_state->matches.load(), m_state->currentPath());
}
