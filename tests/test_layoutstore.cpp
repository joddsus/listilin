#include <QtTest/QtTest>

#include "layoutstore.h"

class TestLayoutStore : public QObject
{
    Q_OBJECT
private slots:
    void validName();
    void serializeRoundTrip();
    void invalidDeserialize();
    void serializeRemovesPasswords();
    void keysAreUniqueForSimilarNames();
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

void TestLayoutStore::serializeRemovesPasswords()
{
    LayoutStore::Layout layout;
    layout.name = QStringLiteral("Server");
    layout.panelCount = 2;
    layout.sourceUri = QStringLiteral("sftp://user:secret@host/src");
    layout.destUri = QStringLiteral("ftp://user:other@host/dst");
    const QString text = LayoutStore::serialize(layout);
    QVERIFY(!text.contains(QStringLiteral("secret")));
    QVERIFY(!text.contains(QStringLiteral("other")));

    LayoutStore::Layout parsed;
    QVERIFY(LayoutStore::deserialize(text, &parsed));
    QCOMPARE(parsed.sourceUri, QStringLiteral("sftp://user@host/src"));
    QCOMPARE(parsed.destUri, QStringLiteral("ftp://user@host/dst"));
}

void TestLayoutStore::keysAreUniqueForSimilarNames()
{
    // Two names a user may reasonably choose must not share a settings slot.
    const QString spaced = LayoutStore::keyFor(QStringLiteral("Min layout"));
    const QString underscored = LayoutStore::keyFor(QStringLiteral("Min_layout"));
    QVERIFY(spaced != underscored);
    QVERIFY(!spaced.contains(QLatin1Char('/')));
    QVERIFY(!underscored.contains(QLatin1Char('/')));
    // The same name keeps the same key, so saving again updates its layout.
    QCOMPARE(spaced, LayoutStore::keyFor(QStringLiteral("Min layout")));
}

QTEST_MAIN(TestLayoutStore)
#include "test_layoutstore.moc"