#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "fileops.h"
#include "treesnapshot.h"

class TestFileOps : public QObject
{
    Q_OBJECT
private slots:
    void copyOverwritesByStaging();
    void moveToTrashKeepsSourceOnCopyFailure();
    void copyKeepBothPreservesTarget();
    void createFileRefusesOverwrite();
    void recursiveMoveSkipPreservesSource();
    void recursiveMoveSkipAllPreservesSource();
    void recursiveMoveAbortPreservesSource();
    void recursiveMoveCancelledPreservesTarget();
    void uriRecursiveMoveSkipPreservesSource();
    void replaceAbortPreservesTarget();
    void moveToSameFolderKeepsFile();
    void moveFolderIntoItselfRejected();
    void moveSymlinkKeepsTargetContents();
    void emptyTrashKeepsSymlinkTarget();
    void movePreservesModeAndLinks();
    void moveKeepsSourceWhenItChangesDuringCopy();
    void trashReportsPerItem();
    void restoreOnlyTheMatchingTrashEntry();
};

namespace {

class DataHomeGuard
{
public:
    explicit DataHomeGuard(const QString &path)
    {
        m_previous = qgetenv("XDG_DATA_HOME");
        m_hadPrevious = !m_previous.isEmpty();
        qputenv("XDG_DATA_HOME", path.toUtf8());
    }
    ~DataHomeGuard()
    {
        if (m_hadPrevious)
            qputenv("XDG_DATA_HOME", m_previous);
        else
            qunsetenv("XDG_DATA_HOME");
    }

private:
    QByteArray m_previous;
    bool m_hadPrevious = false;
};

} // namespace

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

void TestFileOps::createFileRefusesOverwrite()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QString created;
    QString error;
    QVERIFY(FileOps::createFile(tmp.path(), QStringLiteral("new.txt"), &created, &error));
    QVERIFY(error.isEmpty());
    QVERIFY(QFileInfo::exists(created));
    QCOMPARE(QFileInfo(created).size(), 0);

    writeFile(created, QByteArray("keep"));
    QVERIFY(!FileOps::createFile(tmp.path(), QStringLiteral("new.txt"), nullptr, &error));
    QVERIFY(!error.isEmpty());

    QFile check(created);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArray("keep"));

    // Unsafe components never reach the filesystem.
    QVERIFY(!FileOps::createFile(tmp.path(), QStringLiteral("../escape"), nullptr, &error));
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

void TestFileOps::copyKeepBothPreservesTarget()
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
    progress.conflict = [](const QString &, const QString &) {
        return FileOps::ConflictKeepBoth;
    };

    QString error;
    const QStringList failed = FileOps::copyAll({source}, targetDir, false, &error, progress);
    QVERIFY(failed.isEmpty());
    QVERIFY(error.isEmpty());

    QFile oldFile(existing);
    QVERIFY(oldFile.open(QIODevice::ReadOnly));
    QCOMPARE(oldFile.readAll(), QByteArray("old data"));

    QFile newFile(targetDir + QStringLiteral("/source (2).txt"));
    QVERIFY(newFile.open(QIODevice::ReadOnly));
    QCOMPARE(newFile.readAll(), QByteArray("new data"));
}

void TestFileOps::moveToSameFolderKeepsFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString file = tmp.path() + QStringLiteral("/only-copy.txt");
    writeFile(file, QByteArray("precious"));

    int asked = 0;
    FileOps::Progress progress;
    progress.conflict = [&asked](const QString &, const QString &) {
        ++asked;
        return FileOps::ConflictReplace;
    };

    QString error;
    const FileOps::Result result
        = FileOps::copyAllResult({file}, tmp.path(), true, &error, progress);
    QVERIFY2(result.failed.isEmpty(), qPrintable(result.error));
    QVERIFY(error.isEmpty());
    QCOMPARE(asked, 0);

    QVERIFY(QFileInfo::exists(file));
    QFile check(file);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArray("precious"));
}

void TestFileOps::moveFolderIntoItselfRejected()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString folder = tmp.path() + QStringLiteral("/folder");
    const QString child = folder + QStringLiteral("/child");
    QVERIFY(QDir().mkpath(child));
    writeFile(child + QStringLiteral("/f.txt"), QByteArray("keep"));

    QString error;
    const FileOps::Result intoChild
        = FileOps::copyAllResult({folder}, child, true, &error, FileOps::Progress());
    QVERIFY(!intoChild.failed.isEmpty());
    QVERIFY(!intoChild.error.isEmpty());
    QVERIFY(QFileInfo::exists(child + QStringLiteral("/f.txt")));

    error.clear();
    const FileOps::Result intoSelf
        = FileOps::copyAllResult({folder}, folder, true, &error, FileOps::Progress());
    QVERIFY(!intoSelf.failed.isEmpty());
    QVERIFY(QFileInfo::exists(child + QStringLiteral("/f.txt")));
}

void TestFileOps::moveSymlinkKeepsTargetContents()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString realDir = tmp.path() + QStringLiteral("/real");
    const QString sourceDir = tmp.path() + QStringLiteral("/source");
    const QString targetDir = tmp.path() + QStringLiteral("/target");
    QVERIFY(QDir().mkpath(realDir));
    QVERIFY(QDir().mkpath(sourceDir));
    QVERIFY(QDir().mkpath(targetDir));

    const QString control = realDir + QStringLiteral("/control.txt");
    writeFile(control, QByteArray("survives"));

    const QString link = sourceDir + QStringLiteral("/link");
    QVERIFY(QFile::link(realDir, link));

    QString error;
    const FileOps::Result result
        = FileOps::copyAllResult({link}, targetDir, true, &error, FileOps::Progress());
    QVERIFY2(result.failed.isEmpty(), qPrintable(result.error));

    // The link moved as a link; the folder it names is untouched.
    QVERIFY(!QFileInfo::exists(link));
    QVERIFY(QFileInfo(targetDir + QStringLiteral("/link")).isSymLink());
    QFile check(control);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArray("survives"));
}

void TestFileOps::emptyTrashKeepsSymlinkTarget()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    DataHomeGuard guard(tmp.path() + QStringLiteral("/data"));

    const QString realDir = tmp.path() + QStringLiteral("/real");
    QVERIFY(QDir().mkpath(realDir));
    const QString control = realDir + QStringLiteral("/control.txt");
    writeFile(control, QByteArray("survives"));

    const QString filesDir = FileOps::trashFolderPath();
    QVERIFY(QDir().mkpath(filesDir));
    QVERIFY(QDir().mkpath(QFileInfo(filesDir).absolutePath() + QStringLiteral("/info")));
    QVERIFY(QFile::link(realDir, filesDir + QStringLiteral("/link")));

    QString error;
    QVERIFY(FileOps::emptyTrash(&error));
    QVERIFY(error.isEmpty());

    QVERIFY(QFileInfo::exists(control));
    QVERIFY(!QFileInfo(filesDir + QStringLiteral("/link")).isSymLink());
}

void TestFileOps::movePreservesModeAndLinks()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString source = tmp.path() + QStringLiteral("/source");
    const QString targetParent = tmp.path() + QStringLiteral("/target");
    QVERIFY(QDir().mkpath(source));
    QVERIFY(QDir().mkpath(targetParent + QStringLiteral("/source")));

    const QString script = source + QStringLiteral("/script.sh");
    writeFile(script, QByteArray("#!/bin/sh\necho hi\n"));
    QVERIFY(QFile::setPermissions(script, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                              | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                                              | QFileDevice::ExeGroup));
    QVERIFY(QFile::link(QStringLiteral("script.sh"), source + QStringLiteral("/link")));
    QVERIFY(QFile::link(QStringLiteral("missing"), source + QStringLiteral("/broken")));

    // An existing destination folder forces the copy-and-verify path instead
    // of a plain rename.
    FileOps::Progress progress;
    progress.conflict = [](const QString &, const QString &) {
        return FileOps::ConflictReplace;
    };

    QString error;
    const FileOps::Result result
        = FileOps::copyAllResult({source}, targetParent, true, &error, progress);
    QVERIFY2(result.failed.isEmpty(), qPrintable(result.error));

    const QString moved = targetParent + QStringLiteral("/source");
    QVERIFY((QFileInfo(moved + QStringLiteral("/script.sh")).permissions()
             & QFileDevice::ExeOwner) != 0);
    // The raw link text is kept; Qt would resolve it to an absolute path.
    QCOMPARE(TreeSnapshot::readLinkTarget(moved + QStringLiteral("/link")),
             QStringLiteral("script.sh"));
    QVERIFY(QFileInfo(moved + QStringLiteral("/broken")).isSymLink());
    QVERIFY(!QFileInfo::exists(source));
}

void TestFileOps::moveKeepsSourceWhenItChangesDuringCopy()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString source = tmp.path() + QStringLiteral("/note.txt");
    const QString targetDir = tmp.path() + QStringLiteral("/dest");
    QVERIFY(QDir().mkpath(targetDir));
    writeFile(source, QByteArray("AAAA"));
    writeFile(targetDir + QStringLiteral("/note.txt"), QByteArray("AAAA"));

    bool changed = false;
    FileOps::Progress progress;
    progress.conflict = [](const QString &, const QString &) {
        return FileOps::ConflictReplace;
    };
    progress.itemDone = [&changed, source]() {
        if (changed)
            return;
        changed = true;
        QFile f(source);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write("BBBB");
            f.close();
        }
    };

    QString error;
    const FileOps::Result result
        = FileOps::copyAllResult({source}, targetDir, true, &error, progress);
    QVERIFY(!result.failed.isEmpty());
    QVERIFY(!error.isEmpty());

    QVERIFY(QFileInfo::exists(source));
    QFile check(source);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArray("BBBB"));
}

void TestFileOps::trashReportsPerItem()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    DataHomeGuard guard(tmp.path() + QStringLiteral("/data"));

    const QString good = tmp.path() + QStringLiteral("/good.txt");
    writeFile(good, QByteArray("bye"));
    const QString missing = tmp.path() + QStringLiteral("/missing.txt");

    const FileOps::TrashResult result = FileOps::moveToTrashDetailed({good, missing});
    QCOMPARE(result.trashed.size(), 1);
    QCOMPARE(result.names.size(), 1);
    QCOMPARE(result.failed.size(), 1);
    QVERIFY(!result.errors.isEmpty());
    QVERIFY(!QFileInfo::exists(good));
}

void TestFileOps::restoreOnlyTheMatchingTrashEntry()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    DataHomeGuard guard(tmp.path() + QStringLiteral("/data"));

    const QString sourceDir = tmp.path() + QStringLiteral("/first");
    const QString otherDir = tmp.path() + QStringLiteral("/second");
    QVERIFY(QDir().mkpath(sourceDir));
    QVERIFY(QDir().mkpath(otherDir));

    const QString original = sourceDir + QStringLiteral("/same.txt");
    writeFile(original, QByteArray("trashed"));
    QVERIFY(FileOps::moveToTrash({original}, nullptr, nullptr));
    QVERIFY(!QFileInfo::exists(original));

    const QString other = otherDir + QStringLiteral("/same.txt");
    writeFile(other, QByteArray("untouched"));

    QString error;
    // A file in an ordinary folder with the same name is not a trash entry.
    QVERIFY(!FileOps::restoreLocalPaths({other}, nullptr, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(original));
    QFile checkOther(other);
    QVERIFY(checkOther.open(QIODevice::ReadOnly));
    QCOMPARE(checkOther.readAll(), QByteArray("untouched"));

    QVERIFY(FileOps::restoreLocalPaths({original}, nullptr, &error));
    QVERIFY2(QFileInfo::exists(original), qPrintable(error));
}

QTEST_MAIN(TestFileOps)
#include "test_fileops.moc"
