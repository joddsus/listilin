#include "pathsafety.h"

#include <QtTest>

class TestPathSafety : public QObject
{
    Q_OBJECT

private slots:
    void safeComponents()
    {
        QVERIFY(PathSafety::isSafePathComponent(QStringLiteral("foo.txt")));
        QVERIFY(PathSafety::isSafePathComponent(QStringLiteral("a-b_c")));
    }

    void unsafeComponents()
    {
        QVERIFY(!PathSafety::isSafePathComponent(QString()));
        QVERIFY(!PathSafety::isSafePathComponent(QStringLiteral(".")));
        QVERIFY(!PathSafety::isSafePathComponent(QStringLiteral("..")));
        QVERIFY(!PathSafety::isSafePathComponent(QStringLiteral("a/b")));
        QVERIFY(!PathSafety::isSafePathComponent(QStringLiteral("CON")));
        QVERIFY(!PathSafety::isSafePathComponent(QStringLiteral("CON.txt")));
        QVERIFY(!PathSafety::isSafePathComponent(QString(300, QLatin1Char('a'))));
    }

    void relativePaths()
    {
        QVERIFY(PathSafety::isSafeRelativePath(QStringLiteral("a/b/c")));
        QVERIFY(!PathSafety::isSafeRelativePath(QStringLiteral("../secret")));
        QVERIFY(!PathSafety::isSafeRelativePath(QStringLiteral("/abs")));
        QVERIFY(PathSafety::isSafeRelativePath(QString()));
    }

    void sanitize()
    {
        QCOMPARE(PathSafety::sanitizePathComponent(QStringLiteral("a/b:c")), QStringLiteral("a_b_c"));
        QCOMPARE(PathSafety::sanitizePathComponent(QStringLiteral("CON")), QStringLiteral("CON_"));
        QVERIFY(!PathSafety::sanitizePathComponent(QString()).isEmpty() == false);
    }

    void staysWithin()
    {
        const QString base = QStringLiteral("/tmp/dest");
        QVERIFY(PathSafety::staysWithin(base, base));
        QVERIFY(PathSafety::staysWithin(base, QStringLiteral("/tmp/dest/file.txt")));
        QVERIFY(PathSafety::staysWithin(base, QStringLiteral("/tmp/dest/sub/x")));
        QVERIFY(!PathSafety::staysWithin(base, QStringLiteral("/tmp/other")));
        QVERIFY(!PathSafety::staysWithin(base, QStringLiteral("/tmp/dest/../other")));
        QVERIFY(!PathSafety::staysWithin(base, QStringLiteral("/")));
    }

    void safeJoin()
    {
        const QString base = QStringLiteral("/tmp/dest");
        QCOMPARE(PathSafety::safeJoin(base, QStringLiteral("file.txt")),
                 QStringLiteral("/tmp/dest/file.txt"));
        QVERIFY(PathSafety::safeJoin(base, QStringLiteral("../x")).isEmpty());
        QVERIFY(PathSafety::safeJoin(base, QStringLiteral("a/b")).isEmpty());
        QVERIFY(PathSafety::safeJoin(base, QString()).isEmpty());
        QVERIFY(PathSafety::safeJoin(base, QStringLiteral("CON")).isEmpty());
    }

    void split()
    {
        const QStringList parts = PathSafety::splitPath(QStringLiteral("a\\b/c"));
        QCOMPARE(parts.size(), 3);
        QCOMPARE(parts.at(0), QStringLiteral("a"));
        QCOMPARE(parts.at(1), QStringLiteral("b"));
        QCOMPARE(parts.at(2), QStringLiteral("c"));
    }
};

QTEST_MAIN(TestPathSafety)
#include "test_pathsafety.moc"