#pragma once

#include <QString>
#include <QList>

#include <cstdint>

// Model for individual job reports (Listi 0.32 "Operations"). Kept free of
// widgets so the state machine can be tested without a GUI.
namespace JobReport {

enum class State {
    Waiting,
    Running,
    Completed,
    Failed,
    Cancelled,
};

struct Report
{
    QString id;
    QString title;
    State state = State::Waiting;
    qint64 bytesDone = 0;
    qint64 bytesTotal = 0;
    int itemsDone = 0;
    int itemsTotal = 0;
    QString error;
    qint64 startedMs = 0;
    qint64 finishedMs = 0;
};

class Store
{
public:
    // Creates a waiting report and returns its id.
    QString enqueue(const QString &title);

    void start(const QString &id, qint64 nowMs = 0);
    void progress(const QString &id, qint64 bytesDone, qint64 bytesTotal,
                  int itemsDone, int itemsTotal);
    void complete(const QString &id, qint64 nowMs = 0);
    void fail(const QString &id, const QString &error, qint64 nowMs = 0);
    void cancel(const QString &id, qint64 nowMs = 0);

    bool contains(const QString &id) const;
    Report find(const QString &id) const;
    QList<Report> all() const;
    QList<Report> waiting() const;
    QList<Report> running() const;
    QList<Report> completed() const; // completed + failed + cancelled

    void clearCompleted();

    int count() const;

private:
    QList<Report> m_reports;
    int m_counter = 0;
};

} // namespace JobReport