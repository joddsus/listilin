#include "jobreport.h"

namespace JobReport {

QString Store::enqueue(const QString &title)
{
    Report report;
    report.id = QStringLiteral("job-%1").arg(++m_counter);
    report.title = title;
    report.state = State::Waiting;
    m_reports.append(report);
    return report.id;
}

void Store::start(const QString &id, qint64 nowMs)
{
    for (Report &report : m_reports) {
        if (report.id != id)
            continue;
        report.state = State::Running;
        if (report.startedMs == 0)
            report.startedMs = nowMs;
        return;
    }
}

void Store::progress(const QString &id, qint64 bytesDone, qint64 bytesTotal,
                     int itemsDone, int itemsTotal)
{
    for (Report &report : m_reports) {
        if (report.id != id)
            continue;
        report.bytesDone = bytesDone;
        report.bytesTotal = bytesTotal;
        report.itemsDone = itemsDone;
        report.itemsTotal = itemsTotal;
        return;
    }
}

void Store::complete(const QString &id, qint64 nowMs)
{
    for (Report &report : m_reports) {
        if (report.id != id)
            continue;
        report.state = State::Completed;
        report.finishedMs = nowMs;
        if (report.bytesTotal > 0)
            report.bytesDone = report.bytesTotal;
        if (report.itemsTotal > 0)
            report.itemsDone = report.itemsTotal;
        return;
    }
}

void Store::fail(const QString &id, const QString &error, qint64 nowMs)
{
    for (Report &report : m_reports) {
        if (report.id != id)
            continue;
        report.state = State::Failed;
        report.error = error;
        report.finishedMs = nowMs;
        return;
    }
}

void Store::cancel(const QString &id, qint64 nowMs)
{
    for (Report &report : m_reports) {
        if (report.id != id)
            continue;
        report.state = State::Cancelled;
        report.finishedMs = nowMs;
        return;
    }
}

bool Store::contains(const QString &id) const
{
    for (const Report &report : m_reports) {
        if (report.id == id)
            return true;
    }
    return false;
}

Report Store::find(const QString &id) const
{
    for (const Report &report : m_reports) {
        if (report.id == id)
            return report;
    }
    return Report();
}

QList<Report> Store::all() const
{
    return m_reports;
}

QList<Report> Store::waiting() const
{
    QList<Report> result;
    for (const Report &report : m_reports) {
        if (report.state == State::Waiting)
            result.append(report);
    }
    return result;
}

QList<Report> Store::running() const
{
    QList<Report> result;
    for (const Report &report : m_reports) {
        if (report.state == State::Running)
            result.append(report);
    }
    return result;
}

QList<Report> Store::completed() const
{
    QList<Report> result;
    for (const Report &report : m_reports) {
        if (report.state == State::Completed || report.state == State::Failed
            || report.state == State::Cancelled)
            result.append(report);
    }
    return result;
}

void Store::clearCompleted()
{
    QList<Report> kept;
    for (const Report &report : m_reports) {
        if (report.state == State::Waiting || report.state == State::Running)
            kept.append(report);
    }
    m_reports = kept;
}

int Store::count() const
{
    return m_reports.size();
}

} // namespace JobReport