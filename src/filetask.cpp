#include "filetask.h"

#include <QTimer>
#include <QtConcurrent>

#include <atomic>
#include <mutex>

// Shared with the worker thread. Atomics for the counters, a mutex for the one
// piece of text, so the UI can read a consistent snapshot at any moment.
struct FileTask::State
{
    std::atomic<qint64> bytes{0};
    std::atomic<qint64> total{0};
    std::atomic<int> items{0};
    std::atomic<bool> cancelled{false};

    std::mutex mutex;
    QString current;

    void setCurrent(const QString &name)
    {
        std::lock_guard<std::mutex> lock(mutex);
        current = name;
    }

    QString currentName()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return current;
    }
};

FileTask::FileTask(const QString &title, Job job, QObject *parent)
    : QObject(parent)
    , m_title(title)
    , m_state(std::make_shared<State>())
{
    const std::shared_ptr<State> state = m_state;

    FileOps::Progress progress;
    progress.total = [state](qint64 totalBytes) { state->total.store(totalBytes); };
    progress.step = [state](qint64 bytes) { state->bytes.fetch_add(bytes); };
    progress.itemDone = [state]() { state->items.fetch_add(1); };
    progress.current = [state](const QString &name) { state->setCurrent(name); };
    progress.cancelled = [state]() { return state->cancelled.load(); };

    // Polling beats one queued signal per file: a copy of a hundred thousand
    // files would otherwise flood the event loop and make the UI sluggish again.
    m_timer = new QTimer(this);
    m_timer->setInterval(100);
    connect(m_timer, &QTimer::timeout, this, &FileTask::poll);

    m_watcher = new QFutureWatcher<TaskOutcome>(this);
    connect(m_watcher, &QFutureWatcher<TaskOutcome>::finished, this, [this]() {
        m_timer->stop();
        TaskOutcome outcome = m_watcher->result();
        outcome.cancelled = outcome.cancelled || isCancelled();
        emit finished(outcome);
        deleteLater();
    });

    m_watcher->setFuture(QtConcurrent::run([job, progress, state]() mutable {
        TaskOutcome outcome;
        QString error;
        outcome.failed = job(progress, &error);
        outcome.error = error;
        outcome.cancelled = state->cancelled.load();
        outcome.transferred = state->items.load();
        return outcome;
    }));

    m_timer->start();
}

FileTask::~FileTask()
{
    // Closing the window must not leave a copy running unseen in the background.
    m_state->cancelled.store(true);
}

FileTask *FileTask::start(const QString &title, Job job, QObject *parent)
{
    return new FileTask(title, std::move(job), parent);
}

void FileTask::cancel()
{
    m_state->cancelled.store(true);
}

bool FileTask::isCancelled() const
{
    return m_state->cancelled.load();
}

void FileTask::poll()
{
    emit progress(m_state->bytes.load(), m_state->total.load(), m_state->currentName(),
                  m_state->items.load());
}
