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
    void encodeDecode();
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

void TestShelf::encodeDecode()
{
    Shelf::Entry a;
    a.uri = QStringLiteral("file:///tmp/a.txt");
    a.name = QStringLiteral("a.txt");
    a.isDir = false;
    Shelf::Entry b;
    b.uri = QStringLiteral("file:///tmp/folder");
    b.name = QStringLiteral("folder");
    b.isDir = true;
    QList<Shelf::Entry> entries;
    entries.append(a);
    entries.append(b);

    const QStringList encoded = Shelf::encodeEntries(entries);
    QCOMPARE(encoded.size(), 2);
    const QList<Shelf::Entry> decoded = Shelf::decodeEntries(encoded);
    QCOMPARE(decoded.size(), 2);
    QCOMPARE(decoded.at(0).uri, a.uri);
    QCOMPARE(decoded.at(0).name, a.name);
    QCOMPARE(decoded.at(0).isDir, false);
    QCOMPARE(decoded.at(1).uri, b.uri);
    QCOMPARE(decoded.at(1).isDir, true);

    // Malformed lines are ignored.
    const QList<Shelf::Entry> malformed = Shelf::decodeEntries({QStringLiteral("only-one-field")});
    QVERIFY(malformed.isEmpty());
}

QTEST_MAIN(TestShelf)
#include "test_shelf.moc"