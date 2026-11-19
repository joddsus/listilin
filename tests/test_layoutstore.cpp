#include <QtTest/QtTest>

#include "layoutstore.h"

class TestLayoutStore : public QObject
{
    Q_OBJECT
private slots:
    void validName();
    void serializeRoundTrip();
    void invalidDeserialize();
};

void TestLayoutStore::validName()
{
    QVERIFY(LayoutStore::validName(QStringLiteral("My Layout")));
    QVERIFY(!LayoutStore::validName(QString()));
    QVERIFY(!LayoutStore::validName(QStringLiteral("a/b")));
}

void TestLayoutStore::serializeRoundTrip()
{
    LayoutStore::Layout layout;
    layout.name = QStringLiteral("Work");
    layout.panelCount = 2;
    layout.folderTree = true;
    layout.places = false;
    layout.sourceUri = QStringLiteral("/src");
    layout.destUri = QStringLiteral("/dst");
    const QString text = LayoutStore::serialize(layout);
    LayoutStore::Layout parsed;
    QVERIFY(LayoutStore::deserialize(text, &parsed));
    QCOMPARE(parsed.name, layout.name);
    QCOMPARE(parsed.panelCount, layout.panelCount);
    QCOMPARE(parsed.folderTree, layout.folderTree);
    QCOMPARE(parsed.places, layout.places);
    QCOMPARE(parsed.sourceUri, layout.sourceUri);
    QCOMPARE(parsed.destUri, layout.destUri);
}

void TestLayoutStore::invalidDeserialize()
{
    LayoutStore::Layout parsed;
    QVERIFY(!LayoutStore::deserialize(QStringLiteral("panelCount=2"), &parsed));
}

QTEST_MAIN(TestLayoutStore)
#include "test_layoutstore.moc"