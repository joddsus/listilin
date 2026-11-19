#include <QtTest/QtTest>

#include "jobreport.h"

class TestJobReport : public QObject
{
    Q_OBJECT
private slots:
    void enqueueAndFind();
    void stateTransitions();
    void waitingAndCompleted();
    void clearCompleted();
};

void TestJobReport::enqueueAndFind()
{
    JobReport::Store store;
    const QString id = store.enqueue(QStringLiteral("Copy"));
    QVERIFY(store.contains(id));
    QCOMPARE(store.count(), 1);
    QCOMPARE(store.find(id).title, QStringLiteral("Copy"));
    QCOMPARE(store.find(id).state, JobReport::State::Waiting);
}

void TestJobReport::stateTransitions()
{
    JobReport::Store store;
    const QString id = store.enqueue(QStringLiteral("Copy"));
    store.start(id, 100);
    QCOMPARE(store.find(id).state, JobReport::State::Running);
    QCOMPARE(store.find(id).startedMs, (qint64)100);
    store.progress(id, 50, 100, 1, 2);
    QCOMPARE(store.find(id).bytesDone, (qint64)50);
    store.complete(id, 200);
    QCOMPARE(store.find(id).state, JobReport::State::Completed);
    QCOMPARE(store.find(id).finishedMs, (qint64)200);
    QCOMPARE(store.find(id).bytesDone, (qint64)100);
}

void TestJobReport::waitingAndCompleted()
{
    JobReport::Store store;
    const QString a = store.enqueue(QStringLiteral("A"));
    const QString b = store.enqueue(QStringLiteral("B"));
    store.start(a);
    store.complete(a);
    store.fail(b, QStringLiteral("boom"));
    QCOMPARE(store.waiting().size(), 0);
    QCOMPARE(store.running().size(), 0);
    QCOMPARE(store.completed().size(), 2);
}

void TestJobReport::clearCompleted()
{
    JobReport::Store store;
    const QString id = store.enqueue(QStringLiteral("A"));
    store.complete(id);
    store.clearCompleted();
    QCOMPARE(store.count(), 0);
}

QTEST_MAIN(TestJobReport)
#include "test_jobreport.moc"