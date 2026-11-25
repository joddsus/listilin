#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "fileops.h"

class TestFileOps : public QObject
{
    Q_OBJECT
private slots:
    void copyOverwritesByStaging();
    void moveToTrashKeepsSourceOnCopyFailure();
    void recursiveMoveSkipPreservesSource();
    void recursiveMoveSkipAllPreservesSource();
    void recursiveMoveAbortPreservesSource();
    void recursiveMoveCancelledPreservesTarget();
    void uriRecursiveMoveSkipPreservesSource();
    void replaceAbortPreservesTarget();
};

void TestFileOps::copyOverwritesByStaging()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString source = tmp.path() + QStringLiteral("/source.txt");
    const QString targetDir = tmp.path() + QStringLiteral("/dest");
    QVERIFY(QDir().mkpath(targetDir));
    const QString existing = targetDir + QStringLiteral("/source.txt");

    QFile s(source);
    QVERIFY(s.open(QIODevice::WriteOnly | QIODevice::Truncate));
    s.write("new data");
    s.close();

    QFile e(existing);
    QVERIFY(e.open(QIODevice::WriteOnly | QIODevice::Truncate));
    e.write("old data");
    e.close();

    FileOps::Progress progress;
    progress.conflict = [](const QString &, const QString &) {
        return FileOps::ConflictReplace;
    };

    QString error;
    const QStringList failed = FileOps::copyAll({source}, targetDir, false, &error, progress);
    QVERIFY(failed.isEmpty());
    QVERIFY(error.isEmpty());

    FileOps::Result opResult = FileOps::copyAllResult({source}, targetDir, false, &error, progress);
    QVERIFY(opResult.failed.isEmpty());
    QVERIFY(opResult.bytesTotal > 0);
    QCOMPARE(opResult.itemsCopied, 1);

    QFile result(existing);
    QVERIFY(result.open(QIODevice::ReadOnly));
    QCOMPARE(result.readAll(), QByteArray("new data"));
}

void TestFileOps::moveToTrashKeepsSourceOnCopyFailure()
{
    // Smoke test: an existing file can be trashed and disappears from its
    // original location on the same filesystem.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString path = tmp.path() + QStringLiteral("/doomed.txt");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("bye");
    f.close();

    QString error;
    QVERIFY(FileOps::moveToTrash({path}, nullptr, &error));
    QVERIFY(error.isEmpty());
    QVERIFY(!QFileInfo::exists(path));
}

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(f.write(data), data.size());
    f.close();
}

void TestFileOps::recursiveMoveSkipPreservesSource()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString srcParent = tmp.path() + QStringLiteral("/srcparent");
    const QString dstParent = tmp.path() + QStringLiteral("/dstparent");
    const QString src = srcParent + QStringLiteral("/folder");
    const QString dst = dstParent + QStringLiteral("/folder");
    QVERIFY(QDir().mkpath(src));
    QVERIFY(QDir().mkpath(dst));
    writeFile(src + QStringLiteral("/f.txt"), QByteArray("AAAA"));
    writeFile(dst + QStringLiteral("/f.txt"), QByteArray("BBBB"));

    FileOps::Progress progress;
    progress.conflict = [](const QString &, const QString &) {
        return FileOps::ConflictSkip;
    };

    QString error;
    const FileOps::Result result = FileOps::copyAllResult({src}, dstParent, true, &error, progress);
    QVERIFY(result.failed.isEmpty());
    QVERIFY(error.isEmpty());
    QCOMPARE(result.itemsCopied, 0);
    QCOMPARE(result.itemsSkipped, 1);
    QVERIFY(!result.cancelled);

    QVERIFY(QFileInfo::exists(src + QStringLiteral("/f.txt")));
    QFile srcFile(src + QStringLiteral("/f.txt"));
    QVERIFY(srcFile.open(QIODevice::ReadOnly));
    QCOMPARE(srcFile.readAll(), QByteArray("AAAA"));

    QFile dstFile(dst + QStringLiteral("/f.txt"));
    QVERIFY(dstFile.open(QIODevice::ReadOnly));
    QCOMPARE(dstFile.readAll(), QByteArray("BBBB"));
}

void TestFileOps::recursiveMoveSkipAllPreservesSource()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString srcParent = tmp.path() + QStringLiteral("/srcparent");
    const QString dstParent = tmp.path() + QStringLiteral("/dstparent");
    const QString src = srcParent + QStringLiteral("/folder");
    const QString dst = dstParent + QStringLiteral("/folder");
    QVERIFY(QDir().mkpath(src));
    QVERIFY(QDir().mkpath(dst));
    writeFile(src + QStringLiteral("/f.txt"), QByteArray("AAAA"));
    writeFile(dst + QStringLiteral("/f.txt"), QByteArray("BBBB"));

    FileOps::Progress progress;
    progress.conflict = [](const QString &, const QString &) {
        return FileOps::ConflictSkipAll;
    };

    QString error;
    const FileOps::Result result = FileOps::copyAllResult({src}, dstParent, true, &error, progress);
    QVERIFY(result.failed.isEmpty());
    QVERIFY(error.isEmpty());
    QCOMPARE(result.itemsCopied, 0);
    QCOMPARE(result.itemsSkipped, 1);
    QVERIFY(!result.cancelled);

    QVERIFY(QFileInfo::exists(src + QStringLiteral("/f.txt")));
    QFile srcFile(src + QStringLiteral("/f.txt"));
    QVERIFY(srcFile.open(QIODevice::ReadOnly));
    QCOMPARE(srcFile.readAll(), QByteArray("AAAA"));

    QFile dstFile(dst + QStringLiteral("/f.txt"));
    QVERIFY(dstFile.open(QIODevice::ReadOnly));
    QCOMPARE(dstFile.readAll(), QByteArray("BBBB"));
}

void TestFileOps::recursiveMoveAbortPreservesSource()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString srcParent = tmp.path() + QStringLiteral("/srcparent");
    const QString dstParent = tmp.path() + QStringLiteral("/dstparent");
    const QString src = srcParent + QStringLiteral("/folder");
    const QString dst = dstParent + QStringLiteral("/folder");
    QVERIFY(QDir().mkpath(src));
    QVERIFY(QDir().mkpath(dst));
    writeFile(src + QStringLiteral("/f.txt"), QByteArray("AAAA"));
    writeFile(dst + QStringLiteral("/f.txt"), QByteArray("BBBB"));

    bool cancelledFlag = false;
    FileOps::Progress progress;
    progress.cancelled = [&cancelledFlag]() { return cancelledFlag; };
    progress.requestCancel = [&cancelledFlag]() { cancelledFlag = true; };
    progress.conflict = [](const QString &, const QString &) {
        return FileOps::ConflictAbort;
    };

    QString error;
    const FileOps::Result result = FileOps::copyAllResult({src}, dstParent, true, &error, progress);
    QVERIFY(result.cancelled);
    QVERIFY(QFileInfo::exists(src + QStringLiteral("/f.txt")));
    QFile dstFile(dst + QStringLiteral("/f.txt"));
    QVERIFY(dstFile.open(QIODevice::ReadOnly));
    QCOMPARE(dstFile.readAll(), QByteArray("BBBB"));
}

void TestFileOps::recursiveMoveCancelledPreservesTarget()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString srcParent = tmp.path() + QStringLiteral("/srcparent");
    const QString dstParent = tmp.path() + QStringLiteral("/dstparent");
    const QString src = srcParent + QStringLiteral("/folder");
    const QString dst = dstParent + QStringLiteral("/folder");
    QVERIFY(QDir().mkpath(src));
    QVERIFY(QDir().mkpath(dst));
    writeFile(src + QStringLiteral("/f.txt"), QByteArray("AAAA"));
    writeFile(dst + QStringLiteral("/f.txt"), QByteArray("BBBB"));

    FileOps::Progress progress;
    progress.cancelled = []() { return true; };
    progress.conflict = [](const QString &, const QString &) {
        return FileOps::ConflictReplace;
    };

    QString error;
    const FileOps::Result result = FileOps::copyAllResult({src}, dstParent, true, &error, progress);
    QVERIFY(result.cancelled);
    QFile dstFile(dst + QStringLiteral("/f.txt"));
    QVERIFY(dstFile.open(QIODevice::ReadOnly));
    QCOMPARE(dstFile.readAll(), QByteArray("BBBB"));
    QVERIFY(QFileInfo::exists(src + QStringLiteral("/f.txt")));
}

void TestFileOps::uriRecursiveMoveSkipPreservesSource()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString srcParent = tmp.path() + QStringLiteral("/srcparent");
    const QString dstParent = tmp.path() + QStringLiteral("/dstparent");
    const QString src = srcParent + QStringLiteral("/folder");
    const QString dst = dstParent + QStringLiteral("/folder");
    QVERIFY(QDir().mkpath(src));
    QVERIFY(QDir().mkpath(dst));
    writeFile(src + QStringLiteral("/f.txt"), QByteArray("AAAA"));
    writeFile(dst + QStringLiteral("/f.txt"), QByteArray("BBBB"));

    FileOps::Progress progress;
    progress.conflict = [](const QString &, const QString &) {
        return FileOps::ConflictSkip;
    };

    QVector<FileOps::CopyItem> items;
    FileOps::CopyItem item;
    item.uri = QUrl::fromLocalFile(src).toString();
    item.isDir = true;
    items.append(item);

    QString error;
    const FileOps::Result result = FileOps::copyUrisResult(items, dstParent, true, &error, progress);
    QVERIFY(result.failed.isEmpty());
    QVERIFY(error.isEmpty());
    QCOMPARE(result.itemsSkipped, 1);
    QVERIFY(QFileInfo::exists(src + QStringLiteral("/f.txt")));
    QFile dstFile(dst + QStringLiteral("/f.txt"));
    QVERIFY(dstFile.open(QIODevice::ReadOnly));
    QCOMPARE(dstFile.readAll(), QByteArray("BBBB"));
}

void TestFileOps::replaceAbortPreservesTarget()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString source = tmp.path() + QStringLiteral("/source.txt");
    const QString targetDir = tmp.path() + QStringLiteral("/dest");
    QVERIFY(QDir().mkpath(targetDir));
    const QString existing = targetDir + QStringLiteral("/source.txt");

    writeFile(source, QByteArray("new data"));
    writeFile(existing, QByteArray("old data"));

    FileOps::Progress progress;
    progress.cancelled = []() { return true; };
    progress.conflict = [](const QString &, const QString &) {
        return FileOps::ConflictReplace;
    };

    QString error;
    const FileOps::Result result = FileOps::copyAllResult({source}, targetDir, false, &error, progress);
    QVERIFY(result.cancelled);
    QFile resultFile(existing);
    QVERIFY(resultFile.open(QIODevice::ReadOnly));
    QCOMPARE(resultFile.readAll(), QByteArray("old data"));
}

QTEST_MAIN(TestFileOps)
#include "test_fileops.moc"
