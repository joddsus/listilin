#include <QtTest/QtTest>

#include "millercolumns.h"

class TestMillerColumns : public QObject
{
    Q_OBJECT
private slots:
    void rootAndCurrent();
    void selectDescends();
    void goUp();
    void reset();
};

void TestMillerColumns::rootAndCurrent()
{
    MillerColumns::Navigator nav;
    nav.setRoot(QStringLiteral("/root"));
    QCOMPARE(nav.columnCount(), 1);
    QCOMPARE(nav.currentPath(), QStringLiteral("/root"));
    QCOMPARE(nav.pathAt(0), QStringLiteral("/root"));
}

void TestMillerColumns::selectDescends()
{
    MillerColumns::Navigator nav;
    nav.setRoot(QStringLiteral("/root"));
    nav.select(0, QStringLiteral("/root/a"), true);
    QCOMPARE(nav.columnCount(), 2);
    QCOMPARE(nav.currentPath(), QStringLiteral("/root/a"));
}

void TestMillerColumns::goUp()
{
    MillerColumns::Navigator nav;
    nav.setRoot(QStringLiteral("/root"));
    nav.select(0, QStringLiteral("/root/a"), true);
    nav.goUp();
    QCOMPARE(nav.columnCount(), 1);
    QCOMPARE(nav.currentPath(), QStringLiteral("/root"));
}

void TestMillerColumns::reset()
{
    MillerColumns::Navigator nav;
    nav.setRoot(QStringLiteral("/root"));
    nav.select(0, QStringLiteral("/root/a"), true);
    nav.reset();
    QCOMPARE(nav.columnCount(), 1);
    QCOMPARE(nav.currentPath(), QStringLiteral("/root"));
}

QTEST_MAIN(TestMillerColumns)
#include "test_millercolumns.moc"