#include <QtTest/QtTest>

#include "shelf.h"

class TestShelf : public QObject
{
    Q_OBJECT
private slots:
    void addAndCount();
    void addUnique();
    void removeAt();
    void containsAndIndex();
};

void TestShelf::addAndCount()
{
    Shelf::Store store;
    Shelf::Entry e;
    e.uri = QStringLiteral("/a");
    e.name = QStringLiteral("a");
    store.add(e);
    QCOMPARE(store.count(), 1);
    QCOMPARE(store.at(0).uri, QStringLiteral("/a"));
}

void TestShelf::addUnique()
{
    Shelf::Store store;
    Shelf::Entry e;
    e.uri = QStringLiteral("/a");
    store.addUnique(e);
    store.addUnique(e);
    QCOMPARE(store.count(), 1);
}

void TestShelf::removeAt()
{
    Shelf::Store store;
    Shelf::Entry e;
    e.uri = QStringLiteral("/a");
    store.add(e);
    e.uri = QStringLiteral("/b");
    store.add(e);
    store.removeAt(0);
    QCOMPARE(store.count(), 1);
    QCOMPARE(store.at(0).uri, QStringLiteral("/b"));
}

void TestShelf::containsAndIndex()
{
    Shelf::Store store;
    Shelf::Entry e;
    e.uri = QStringLiteral("/a");
    store.add(e);
    QVERIFY(store.contains(QStringLiteral("/a")));
    QCOMPARE(store.indexOf(QStringLiteral("/a")), 0);
    QCOMPARE(store.indexOf(QStringLiteral("/x")), -1);
}

QTEST_MAIN(TestShelf)
#include "test_shelf.moc"