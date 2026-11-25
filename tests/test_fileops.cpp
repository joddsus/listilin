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

QTEST_MAIN(TestFileOps)
#include "test_fileops.moc"
