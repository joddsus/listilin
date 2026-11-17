#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "foldercompare.h"

class TestFolderCompare : public QObject
{
    Q_OBJECT
private slots:
    void identicalTrees();
    void onlyLeftAndRight();
    void sizeChanged();
    void contentChanged();
    void cancel();
};

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
    f.close();
}

void TestFolderCompare::identicalTrees()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());
    writeFile(left.path() + QStringLiteral("/a.txt"), QByteArray("hello"));
    writeFile(right.path() + QStringLiteral("/a.txt"), QByteArray("hello"));

    FolderCompare::Result r = FolderCompare::run(left.path(), right.path(), {});
    QVERIFY(r.error.isEmpty());
    QVERIFY(!r.cancelled);
    QCOMPARE(r.items.size(), 0);
}

void TestFolderCompare::onlyLeftAndRight()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());
    writeFile(left.path() + QStringLiteral("/leftonly.txt"), QByteArray("x"));
    writeFile(right.path() + QStringLiteral("/rightonly.txt"), QByteArray("y"));

    FolderCompare::Result r = FolderCompare::run(left.path(), right.path(), {});
    QCOMPARE(r.items.size(), 2);
    bool sawLeft = false, sawRight = false;
    for (const FolderCompare::Item &it : r.items) {
        if (it.kind == FolderCompare::Difference::OnlyLeft) sawLeft = true;
        if (it.kind == FolderCompare::Difference::OnlyRight) sawRight = true;
    }
    QVERIFY(sawLeft);
    QVERIFY(sawRight);
}

void TestFolderCompare::sizeChanged()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());
    writeFile(left.path() + QStringLiteral("/f.txt"), QByteArray("short"));
    writeFile(right.path() + QStringLiteral("/f.txt"), QByteArray("a bit longer"));

    FolderCompare::Result r = FolderCompare::run(left.path(), right.path(), {});
    QCOMPARE(r.items.size(), 1);
    QCOMPARE(r.items.first().kind, FolderCompare::Difference::SizeChanged);
}

void TestFolderCompare::contentChanged()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());
    writeFile(left.path() + QStringLiteral("/f.txt"), QByteArray("aaaa"));
    writeFile(right.path() + QStringLiteral("/f.txt"), QByteArray("bbbb"));

    FolderCompare::Options opts;
    opts.compareContents = true;
    FolderCompare::Result r = FolderCompare::run(left.path(), right.path(), opts);
    // sizes equal (4 == 4), so content hash differs -> ContentChanged
    QCOMPARE(r.items.size(), 1);
    QCOMPARE(r.items.first().kind, FolderCompare::Difference::ContentChanged);
}

void TestFolderCompare::cancel()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());
    for (int i = 0; i < 50; ++i)
        writeFile(left.path() + QStringLiteral("/f%1.txt").arg(i), QByteArray(1024, 'a'));

    std::atomic_bool cancelFlag{false};
    // cancel immediately
    cancelFlag.store(true);
    FolderCompare::Result r = FolderCompare::run(left.path(), right.path(), {}, &cancelFlag);
    QVERIFY(r.cancelled);
}

QTEST_MAIN(TestFolderCompare)
#include "test_foldercompare.moc"
