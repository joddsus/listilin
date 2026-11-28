#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
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
    void nestedDirectories();
    void typeChanged();
    void emptyTrees();
    void countsAndBytes();
    void hiddenExcluded();
    void relativeLinksCompareEqual();
    void brokenLinksCompareByText();
    void unreadableSubtreeIsIncomplete();
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

void TestFolderCompare::nestedDirectories()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());
    QDir(left.path()).mkpath(QStringLiteral("sub"));
    QDir(right.path()).mkpath(QStringLiteral("sub"));
    writeFile(left.path() + QStringLiteral("/sub/f.txt"), QByteArray("x"));
    writeFile(right.path() + QStringLiteral("/sub/f.txt"), QByteArray("x"));

    FolderCompare::Result r = FolderCompare::run(left.path(), right.path(), {});
    QVERIFY(r.error.isEmpty());
    QCOMPARE(r.items.size(), 0);
}

void TestFolderCompare::typeChanged()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());
    writeFile(left.path() + QStringLiteral("/x"), QByteArray("data"));
    QDir(right.path()).mkpath(QStringLiteral("x"));

    FolderCompare::Result r = FolderCompare::run(left.path(), right.path(), {});
    QCOMPARE(r.items.size(), 1);
    QCOMPARE(r.items.first().kind, FolderCompare::Difference::TypeChanged);
}

void TestFolderCompare::emptyTrees()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());

    FolderCompare::Result r = FolderCompare::run(left.path(), right.path(), {});
    QVERIFY(r.error.isEmpty());
    QVERIFY(!r.cancelled);
    QCOMPARE(r.items.size(), 0);
    QCOMPARE(r.filesCompared, 0);
    QCOMPARE(r.directoriesCompared, 0);
}

void TestFolderCompare::countsAndBytes()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());
    writeFile(left.path() + QStringLiteral("/a.txt"), QByteArray("hello"));
    writeFile(right.path() + QStringLiteral("/a.txt"), QByteArray("hello"));

    FolderCompare::Options opts;
    opts.compareContents = true;
    FolderCompare::Result r = FolderCompare::run(left.path(), right.path(), opts);
    QCOMPARE(r.filesCompared, 1);
    QVERIFY(r.bytesHashed > 0);
    QCOMPARE(r.items.size(), 0);
}

void TestFolderCompare::hiddenExcluded()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());
    writeFile(left.path() + QStringLiteral("/.hidden"), QByteArray("x"));
    writeFile(right.path() + QStringLiteral("/.hidden"), QByteArray("y"));

    FolderCompare::Options opts;
    opts.includeHidden = false;
    FolderCompare::Result r = FolderCompare::run(left.path(), right.path(), opts);
    QVERIFY(r.error.isEmpty());
    QCOMPARE(r.items.size(), 0);
}

void TestFolderCompare::relativeLinksCompareEqual()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());
    for (const QString &root : {left.path(), right.path()}) {
        QVERIFY(QDir(root).mkpath(QStringLiteral("sub")));
        writeFile(root + QStringLiteral("/sub/item"), QByteArray("same"));
        // The same relative link text under two different roots is the same
        // link, even though its absolute target differs.
        QVERIFY(QFile::link(QStringLiteral("sub/item"), root + QStringLiteral("/link")));
    }

    FolderCompare::Result r = FolderCompare::run(left.path(), right.path(), {});
    QVERIFY(r.error.isEmpty());
    QVERIFY(!r.incomplete);
    QCOMPARE(r.items.size(), 0);
}

void TestFolderCompare::brokenLinksCompareByText()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());
    for (const QString &root : {left.path(), right.path()})
        QVERIFY(QFile::link(QStringLiteral("sub/missing"), root + QStringLiteral("/link")));

    FolderCompare::Result same = FolderCompare::run(left.path(), right.path(), {});
    QCOMPARE(same.items.size(), 0);

    QFile::remove(right.path() + QStringLiteral("/link"));
    QVERIFY(QFile::link(QStringLiteral("sub/other"), right.path() + QStringLiteral("/link")));
    FolderCompare::Result different = FolderCompare::run(left.path(), right.path(), {});
    QCOMPARE(different.items.size(), 1);
    QCOMPARE(different.items.first().kind, FolderCompare::Difference::LinkChanged);
}

void TestFolderCompare::unreadableSubtreeIsIncomplete()
{
    QTemporaryDir left;
    QTemporaryDir right;
    QVERIFY(left.isValid() && right.isValid());
    QVERIFY(QDir(left.path()).mkpath(QStringLiteral("sub")));
    QVERIFY(QDir(right.path()).mkpath(QStringLiteral("sub")));
    writeFile(left.path() + QStringLiteral("/sub/only-left.txt"), QByteArray("x"));

    const QString unreadable = left.path() + QStringLiteral("/sub");
    QFile::setPermissions(unreadable, QFile::Permissions());
    if (QFileInfo(unreadable).isReadable()) {
        QFile::setPermissions(unreadable, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        QSKIP("This user can read a mode-000 folder (running as root), so the incomplete "
              "path cannot be exercised here.");
    }

    FolderCompare::Result r = FolderCompare::run(left.path(), right.path(), {});
    QFile::setPermissions(unreadable, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    QVERIFY(r.incomplete);
    QVERIFY(!r.unreadable.isEmpty());
    QVERIFY(r.unreadable.first().contains(QStringLiteral("left")));
    // The hidden file inside must not be reported as a match-worthy result.
    QCOMPARE(r.items.size(), 0);
}

QTEST_MAIN(TestFolderCompare)
#include "test_foldercompare.moc"
