#include "duplicatefinder.h"

#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

class TestDuplicateFinder : public QObject
{
    Q_OBJECT

private slots:
    void noDuplicates()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile f(dir.filePath(QStringLiteral("a.txt")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("one");
        f.close();
        QFile f2(dir.filePath(QStringLiteral("b.txt")));
        QVERIFY(f2.open(QIODevice::WriteOnly));
        f2.write("two");
        f2.close();

        const auto groups = DuplicateFinder::findDuplicates(dir.path());
        QVERIFY(groups.isEmpty());
    }

    void findsDuplicates()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const QString &name : {QStringLiteral("a.txt"), QStringLiteral("b.txt")}) {
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("same content");
            f.close();
        }
        QFile f3(dir.filePath(QStringLiteral("c.txt")));
        QVERIFY(f3.open(QIODevice::WriteOnly));
        f3.write("different");
        f3.close();

        const auto groups = DuplicateFinder::findDuplicates(dir.path());
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups.at(0).paths.size(), 2);
    }

    void ignoresSmallFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const QString &name : {QStringLiteral("a.txt"), QStringLiteral("b.txt")}) {
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
            f.close();
        }

        const auto groups = DuplicateFinder::findDuplicates(dir.path(), 100);
        QVERIFY(groups.isEmpty());
    }
};

QTEST_MAIN(TestDuplicateFinder)
#include "test_duplicatefinder.moc"