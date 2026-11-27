#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <memory>

#include "fileops.h"
#include "vfs.h"
#include "vfs_archive.h"
#include "vfs_local.h"

class TestUploadUndo : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void movedFileIsRestoredAndRemovedFromTheArchive();
    void copiedFileUndoOnlyRemovesTheUpload();
    void folderMoveUndoRestoresTheTreeAndEmptyFolder();
    void changedMemberStopsTheUndo();
    void missingMemberStopsTheUndo();
    void occupiedOriginalNameStopsTheUndo();
    void archiveSourceCopyUndoRemovesOnlyTheDestination();
};

static void registerBackends()
{
    if (!Vfs::instance().backendFor(QStringLiteral("file")))
        Vfs::instance().addBackend(std::make_shared<LocalBackend>());
    if (!Vfs::instance().backendFor(QStringLiteral("archive")))
        Vfs::instance().addBackend(std::make_shared<ArchiveBackend>());
}

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(f.write(data), qint64(data.size()));
    f.close();
}

static QByteArray readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

static QString archiveUri(const QString &path)
{
    return VfsUri::archiveUriFor(QStringLiteral("file://") + path);
}

static QStringList memberNames(const QString &uri)
{
    QVector<Entry> entries;
    QString error;
    if (!Vfs::instance().list(uri, true, &entries, &error))
        return {QStringLiteral("<list failed: ") + error + QLatin1Char('>')};
    QStringList names;
    for (const Entry &entry : entries)
        names.append(entry.name);
    names.sort();
    return names;
}

void TestUploadUndo::initTestCase()
{
    registerBackends();
}

void TestUploadUndo::movedFileIsRestoredAndRemovedFromTheArchive()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/notes.txt");
    const QByteArray payload("the text that must come back");
    writeFile(source, payload);

    const QString targetArchive = tmp.path() + QStringLiteral("/undo.zip");
    const QString target = archiveUri(targetArchive);

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + source;
    QString error;
    const FileOps::Result upload
        = FileOps::uploadUrisResult({item}, target, true, &error, FileOps::Progress());
    QVERIFY2(upload.failed.isEmpty(), qPrintable(error));
    QCOMPARE(upload.uploads.size(), 1);
    QCOMPARE(upload.uploads.first().sourcePath, source);
    QCOMPARE(upload.uploads.first().destinationUri,
             VfsUri::childUri(target, QStringLiteral("notes.txt")));
    QVERIFY(!upload.uploads.first().digest.isEmpty());
    QVERIFY(!QFile::exists(source));

    const FileOps::Result undo
        = FileOps::undoUploadsResult(upload.uploads, &error, FileOps::Progress());
    QVERIFY2(undo.failed.isEmpty(), qPrintable(error));
    QVERIFY2(undo.error.isEmpty(), qPrintable(undo.error));
    QVERIFY(QFile::exists(source));
    QCOMPARE(readFile(source), payload);
    QVERIFY(memberNames(target).isEmpty());
}

void TestUploadUndo::copiedFileUndoOnlyRemovesTheUpload()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/kept.txt");
    writeFile(source, "still mine");

    const QString target = archiveUri(tmp.path() + QStringLiteral("/copy.zip"));

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + source;
    QString error;
    const FileOps::Result upload
        = FileOps::uploadUrisResult({item}, target, false, &error, FileOps::Progress());
    QVERIFY2(upload.failed.isEmpty(), qPrintable(error));
    QCOMPARE(upload.uploads.size(), 1);
    QVERIFY(upload.uploads.first().sourcePath.isEmpty());

    const FileOps::Result undo
        = FileOps::undoUploadsResult(upload.uploads, &error, FileOps::Progress());
    QVERIFY2(undo.failed.isEmpty(), qPrintable(error));
    QVERIFY(QFile::exists(source));
    QCOMPARE(readFile(source), QByteArray("still mine"));
    QVERIFY(memberNames(target).isEmpty());
}

void TestUploadUndo::folderMoveUndoRestoresTheTreeAndEmptyFolder()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString root = tmp.path() + QStringLiteral("/tree");
    QVERIFY(QDir().mkpath(root + QStringLiteral("/sub/empty")));
    writeFile(root + QStringLiteral("/top.txt"), "top");
    writeFile(root + QStringLiteral("/sub/inner.txt"), "inner");

    const QString target = archiveUri(tmp.path() + QStringLiteral("/tree.zip"));
    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + root;
    item.isDir = true;

    QString error;
    const FileOps::Result upload
        = FileOps::uploadUrisResult({item}, target, true, &error, FileOps::Progress());
    QVERIFY2(upload.failed.isEmpty(), qPrintable(error));
    QVERIFY(!QFileInfo::exists(root));

    const FileOps::Result undo
        = FileOps::undoUploadsResult(upload.uploads, &error, FileOps::Progress());
    QVERIFY2(undo.failed.isEmpty(), qPrintable(error));
    QVERIFY2(undo.error.isEmpty(), qPrintable(undo.error));

    QCOMPARE(readFile(root + QStringLiteral("/top.txt")), QByteArray("top"));
    QCOMPARE(readFile(root + QStringLiteral("/sub/inner.txt")), QByteArray("inner"));
    QVERIFY(QFileInfo(root + QStringLiteral("/sub/empty")).isDir());
    QVERIFY(memberNames(target).isEmpty());
}

void TestUploadUndo::changedMemberStopsTheUndo()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/notes.txt");
    writeFile(source, "original text");
    const QString target = archiveUri(tmp.path() + QStringLiteral("/changed.zip"));

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + source;
    QString error;
    const FileOps::Result upload
        = FileOps::uploadUrisResult({item}, target, true, &error, FileOps::Progress());
    QVERIFY2(upload.failed.isEmpty(), qPrintable(error));
    QVERIFY(!QFile::exists(source));

    // Someone replaces the uploaded member before the undo.
    const QString replacement = tmp.path() + QStringLiteral("/replacement.txt");
    writeFile(replacement, "somebody else's text");
    QVERIFY(Vfs::instance().put(target, QStringLiteral("notes.txt"), replacement, &error));

    const FileOps::Result undo
        = FileOps::undoUploadsResult(upload.uploads, &error, FileOps::Progress());
    QCOMPARE(undo.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("changed")), qPrintable(error));
    QVERIFY(!QFile::exists(source));
    QVERIFY(memberNames(target).contains(QStringLiteral("notes.txt")));
}

void TestUploadUndo::missingMemberStopsTheUndo()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/gone.txt");
    writeFile(source, "gone soon");
    const QString target = archiveUri(tmp.path() + QStringLiteral("/gone.zip"));

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + source;
    QString error;
    const FileOps::Result upload
        = FileOps::uploadUrisResult({item}, target, true, &error, FileOps::Progress());
    QVERIFY2(upload.failed.isEmpty(), qPrintable(error));

    QVERIFY(Vfs::instance().remove(VfsUri::childUri(target, QStringLiteral("gone.txt")),
                                   false, &error));

    const FileOps::Result undo
        = FileOps::undoUploadsResult(upload.uploads, &error, FileOps::Progress());
    QCOMPARE(undo.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("read back")), qPrintable(error));
    QVERIFY(!QFile::exists(source));
}

void TestUploadUndo::occupiedOriginalNameStopsTheUndo()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/notes.txt");
    writeFile(source, "first version");
    const QString target = archiveUri(tmp.path() + QStringLiteral("/occupied.zip"));

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + source;
    QString error;
    const FileOps::Result upload
        = FileOps::uploadUrisResult({item}, target, true, &error, FileOps::Progress());
    QVERIFY2(upload.failed.isEmpty(), qPrintable(error));

    // A different file now has the original name.
    writeFile(source, "a new file with the old name");

    const FileOps::Result undo
        = FileOps::undoUploadsResult(upload.uploads, &error, FileOps::Progress());
    QCOMPARE(undo.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("already exists")), qPrintable(error));
    QCOMPARE(readFile(source), QByteArray("a new file with the old name"));
    QVERIFY(memberNames(target).contains(QStringLiteral("notes.txt")));
}

void TestUploadUndo::archiveSourceCopyUndoRemovesOnlyTheDestination()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString firstArchive = archiveUri(tmp.path() + QStringLiteral("/first.zip"));
    const QString memberSource = tmp.path() + QStringLiteral("/member.txt");
    writeFile(memberSource, "inside the first archive");
    QString error;
    QVERIFY(Vfs::instance().put(firstArchive, QStringLiteral("member.txt"), memberSource,
                                &error));

    const QString secondArchive = archiveUri(tmp.path() + QStringLiteral("/second.zip"));
    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(firstArchive, QStringLiteral("member.txt"));
    const FileOps::Result upload
        = FileOps::uploadUrisResult({item}, secondArchive, false, &error, FileOps::Progress());
    QVERIFY2(upload.failed.isEmpty(), qPrintable(error));
    QCOMPARE(upload.uploads.size(), 1);
    QVERIFY(upload.uploads.first().sourcePath.isEmpty());

    const FileOps::Result undo
        = FileOps::undoUploadsResult(upload.uploads, &error, FileOps::Progress());
    QVERIFY2(undo.failed.isEmpty(), qPrintable(error));
    QVERIFY(memberNames(secondArchive).isEmpty());
    QVERIFY(memberNames(firstArchive).contains(QStringLiteral("member.txt")));
}

QTEST_MAIN(TestUploadUndo)
#include "test_uploadundo.moc"
