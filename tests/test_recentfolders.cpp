#include <QtTest/QtTest>

#include "recentfolders.h"

class TestRecentFolders : public QObject
{
    Q_OBJECT
private slots:
    void newestFirstWithoutDuplicates();
    void excludesCurrentAndHonoursLimit();
};

void TestRecentFolders::newestFirstWithoutDuplicates()
{
    const QStringList recent = RecentFolders::mostRecent(
        {QStringLiteral("/a"), QStringLiteral("/b"), QStringLiteral("/a"),
         QStringLiteral("/c")},
        10, QStringLiteral("/current"));
    QCOMPARE(recent, QStringList({QStringLiteral("/c"), QStringLiteral("/a"),
                                  QStringLiteral("/b")}));
}

void TestRecentFolders::excludesCurrentAndHonoursLimit()
{
    const QStringList history = {QStringLiteral("/a"), QStringLiteral("/b"),
                                 QStringLiteral("/c"), QStringLiteral("/d")};
    const QStringList recent = RecentFolders::mostRecent(history, 2, QStringLiteral("/d"));
    QCOMPARE(recent, QStringList({QStringLiteral("/c"), QStringLiteral("/b")}));
    QVERIFY(!recent.contains(QStringLiteral("/d")));
}

QTEST_MAIN(TestRecentFolders)
#include "test_recentfolders.moc"
