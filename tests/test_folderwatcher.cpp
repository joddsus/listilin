#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "folderwatcher.h"

class TestFolderWatcher : public QObject
{
    Q_OBJECT
private slots:
    void reportsAChangeInTheWatchedFolder();
    void ignoresMissingFolders();
    void reportsOnceForABurst();
};

void TestFolderWatcher::reportsAChangeInTheWatchedFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FolderWatcher watcher;
    QSignalSpy changed(&watcher, &FolderWatcher::folderChanged);
    watcher.setFolder(dir.path());
    QCOMPARE(watcher.folder(), QDir(dir.path()).absolutePath());

    QFile file(QDir(dir.path()).filePath(QStringLiteral("new.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hello");
    file.close();

    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 5000);
    QCOMPARE(changed.first().first().toString(), QDir(dir.path()).absolutePath());
}

void TestFolderWatcher::ignoresMissingFolders()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FolderWatcher watcher;
    QSignalSpy changed(&watcher, &FolderWatcher::folderChanged);
    watcher.setFolder(dir.path() + QStringLiteral("/absent"));
    QCOMPARE(watcher.folder(), dir.path() + QStringLiteral("/absent"));

    watcher.setFolder(QString());
    QVERIFY(watcher.folder().isEmpty());
    QTest::qWait(400);
    QCOMPARE(changed.count(), 0);
}

void TestFolderWatcher::reportsOnceForABurst()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    FolderWatcher watcher;
    QSignalSpy changed(&watcher, &FolderWatcher::folderChanged);
    watcher.setFolder(dir.path());

    for (int i = 0; i < 5; ++i) {
        QFile file(QDir(dir.path()).filePath(QStringLiteral("file%1").arg(i)));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
        file.close();
    }

    QTRY_VERIFY_WITH_TIMEOUT(changed.count() >= 1, 5000);
    // The debounce interval is 250 ms, so a tight burst must not produce one
    // signal per file.
    QTest::qWait(400);
    QCOMPARE(changed.count(), 1);
}

QTEST_GUILESS_MAIN(TestFolderWatcher)
#include "test_folderwatcher.moc"
