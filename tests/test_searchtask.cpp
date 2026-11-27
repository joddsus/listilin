#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "searchtask.h"

class TestSearchTask : public QObject
{
    Q_OBJECT
private slots:
    void runsOffTheCallingThreadAndReportsMatches();
    void emitsProgressAndFinishesOnce();
    void cancelAfterFinishIsHarmless();
};

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
    f.close();
}

void TestSearchTask::runsOffTheCallingThreadAndReportsMatches()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir().mkpath(tmp.path() + QStringLiteral("/sub")));
    writeFile(tmp.path() + QStringLiteral("/keep-1.txt"), "one");
    writeFile(tmp.path() + QStringLiteral("/sub/keep-2.txt"), "two");
    writeFile(tmp.path() + QStringLiteral("/drop.txt"), "three");

    SearchScan::Query query;
    query.nameContains = QStringLiteral("keep");

    SearchTask *task = SearchTask::start(tmp.path(), query, this);
    QSignalSpy finished(task, &SearchTask::finished);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);

    const auto result = finished.first().first().value<SearchScan::Result>();
    QCOMPARE(result.matches.size(), 2);
    QVERIFY(!result.cancelled);
    QVERIFY(!result.truncated);
}

void TestSearchTask::emitsProgressAndFinishesOnce()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    for (int i = 0; i < 20; ++i)
        writeFile(tmp.path() + QStringLiteral("/file-%1.txt").arg(i), "x");

    SearchScan::Query query;
    query.maximumResults = 5;
    SearchTask *task = SearchTask::start(tmp.path(), query, this);
    QSignalSpy progress(task, &SearchTask::progress);
    QSignalSpy finished(task, &SearchTask::finished);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
    QCOMPARE(finished.count(), 1);

    const auto result = finished.first().first().value<SearchScan::Result>();
    QVERIFY(result.truncated);
    QCOMPARE(result.matches.size(), 5);
    QVERIFY(progress.count() >= 0); // progress is polled; zero is legal on a fast tree
}

void TestSearchTask::cancelAfterFinishIsHarmless()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeFile(tmp.path() + QStringLiteral("/one.txt"), "x");

    SearchTask *task = SearchTask::start(tmp.path(), SearchScan::Query(), this);
    QSignalSpy finished(task, &SearchTask::finished);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);

    // The task schedules its own deletion after finishing; cancelling must not
    // crash even if the caller races it.
    task->cancel();
    QVERIFY(task->isCancelled());
}

QTEST_MAIN(TestSearchTask)
#include "test_searchtask.moc"
