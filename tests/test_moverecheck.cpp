#include <QtTest/QtTest>

#include "moverecheck.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

class TestMoveRecheck : public QObject
{
    Q_OBJECT
private slots:
    void identicalSnapshotsMatch();
    void changedSizeDetected();
    void changedMtimeDetected();
    void missingPathReported();
    void statPathReadsFile();
};

void TestMoveRecheck::identicalSnapshotsMatch()
{
    MoveRecheck::Stat a;
    a.exists = true;
    a.size = 10;
    a.mtimeMs = 5;
    a.deviceId = 7;
    a.inode = 9;

    MoveRecheck::Stat b = a;
    QVERIFY(MoveRecheck::same(a, b));
    QVERIFY(!MoveRecheck::changed(a, b));
}

void TestMoveRecheck::changedSizeDetected()
{
    MoveRecheck::Stat a;
    a.exists = true;
    a.size = 10;
    MoveRecheck::Stat b = a;
    b.size = 11;
    QVERIFY(MoveRecheck::changed(a, b));
}

void TestMoveRecheck::changedMtimeDetected()
{
    MoveRecheck::Stat a;
    a.exists = true;
    a.mtimeMs = 5;
    MoveRecheck::Stat b = a;
    b.mtimeMs = 6;
    QVERIFY(MoveRecheck::changed(a, b));
}

void TestMoveRecheck::missingPathReported()
{
    MoveRecheck::Stat s = MoveRecheck::statPath(QStringLiteral("/definitely/missing/path"));
    QVERIFY(!s.exists);
}

void TestMoveRecheck::statPathReadsFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("a.txt"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("hello");
    f.close();

    MoveRecheck::Stat s = MoveRecheck::statPath(path);
    QVERIFY(s.exists);
    QCOMPARE(s.size, qint64(5));
}

QTEST_MAIN(TestMoveRecheck)
#include "test_moverecheck.moc"