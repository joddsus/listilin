#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

#include "fileops.h"

struct TaskOutcome
{
    QStringList failed;
    QString error;
    bool cancelled = false;
    int transferred = 0;
    qint64 bytesTransferred = 0;
    qint64 bytesTotal = 0;
};

Q_DECLARE_METATYPE(TaskOutcome)

// Runs one long operation on a pooled thread and reports back on the UI thread.
// The window keeps painting and responding, and the user can stop the job, which
// is the whole difference between "slow" and "hung".
class FileTask : public QObject
{
    Q_OBJECT

public:
    // Returns the URIs that failed; sets error to something a person can read.
    using Job = std::function<QStringList(FileOps::Progress &progress, QString *error)>;

    static FileTask *start(const QString &title, Job job, QObject *parent = nullptr);
    ~FileTask() override;

    QString title() const { return m_title; }
    void cancel();
    void pause();
    void resume();
    bool isPaused() const;
    bool isCancelled() const;

signals:
    void progress(qint64 bytes, qint64 total, const QString &current, int items);
    void finished(const TaskOutcome &outcome);

private:
    struct State;
    class QTimer *m_timer = nullptr;
    QFutureWatcher<TaskOutcome> *m_watcher = nullptr;

    FileTask(const QString &title, Job job, QObject *parent);
    void poll();

    QString m_title;
    std::shared_ptr<State> m_state;
};
