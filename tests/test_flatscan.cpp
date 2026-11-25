#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "flatscan.h"

class TestFlatScan : public QObject
{
    Q_OBJECT
private slots:
    void listsNestedEntriesWithRelativeNames();
    void hidesHiddenEntriesUnlessRequested();
    void rejectsNonFolderRoots();
};

static void writeFile(const QString &path, const QByteArray &data = QByteArray("x"))
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(data), data.size());
    file.close();
}

void TestFlatScan::listsNestedEntriesWithRelativeNames()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir().mkpath(tmp.path() + QStringLiteral("/sub/deep")));
    writeFile(tmp.path() + QStringLiteral("/root.txt"), QByteArray("root"));
    writeFile(tmp.path() + QStringLiteral("/sub/nested.txt"), QByteArray("nested"));
    writeFile(tmp.path() + QStringLiteral("/sub/deep/last.txt"), QByteArray("last"));

    QString error;
    const QVector<Entry> entries = FlatScan::scanLocal(tmp.path(), false, &error);
    QVERIFY(error.isEmpty());

    QStringList names;
    for (const Entry &entry : entries)
        names.append(entry.name);
    names.sort();

    QVERIFY(names.contains(QStringLiteral("root.txt")));
    QVERIFY(names.contains(QStringLiteral("sub")));
    QVERIFY(names.contains(QStringLiteral("sub/nested.txt")));
    QVERIFY(names.contains(QStringLiteral("sub/deep/last.txt")));

    for (const Entry &entry : entries) {
        if (entry.name == QStringLiteral("sub/nested.txt")) {
            QCOMPARE(entry.size, 6);
            QCOMPARE(entry.isDir, false);
        }
        if (entry.name == QStringLiteral("sub"))
            QCOMPARE(entry.isDir, true);
    }
}

void TestFlatScan::hidesHiddenEntriesUnlessRequested()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeFile(tmp.path() + QStringLiteral("/visible.txt"));
    writeFile(tmp.path() + QStringLiteral("/.hidden.txt"));

    const QVector<Entry> hiddenOff = FlatScan::scanLocal(tmp.path(), false);
    QStringList names;
    for (const Entry &entry : hiddenOff)
        names.append(entry.name);
    QVERIFY(names.contains(QStringLiteral("visible.txt")));
    QVERIFY(!names.contains(QStringLiteral(".hidden.txt")));

    const QVector<Entry> hiddenOn = FlatScan::scanLocal(tmp.path(), true);
    names.clear();
    for (const Entry &entry : hiddenOn)
        names.append(entry.name);
    QVERIFY(names.contains(QStringLiteral(".hidden.txt")));
}

void TestFlatScan::rejectsNonFolderRoots()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/file.txt");
    writeFile(path);

    QString error;
    const QVector<Entry> entries = FlatScan::scanLocal(path, false, &error);
    QVERIFY(entries.isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(TestFlatScan)
#include "test_flatscan.moc"
