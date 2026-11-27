#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <memory>

#include "fileops.h"
#include "vfs.h"
#include "vfs_archive.h"

class TestArchiveCopy : public QObject
{
    Q_OBJECT
private slots:
    void archiveMemberCannotReplaceOwnArchive();
    void memberExtractsToAnotherFolder();
};

static void registerArchiveBackend()
{
    // The copy path materialises archive members through the VFS registry.
    if (!Vfs::instance().backendFor(QStringLiteral("archive")))
        Vfs::instance().addBackend(std::make_shared<ArchiveBackend>());
}

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
    f.close();
}

QTEST_MAIN(TestArchiveCopy)
#include "test_archivecopy.moc"

void TestArchiveCopy::archiveMemberCannotReplaceOwnArchive()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/backup.zip");

    const QString payload = tmp.path() + QStringLiteral("/payload.txt");
    writeFile(payload, QByteArray("member contents"));

    ArchiveBackend backend;
    const VfsUri root = VfsUri::parse(QStringLiteral("archive://") + archivePath);
    QString err;
    QVERIFY2(backend.put(root, QStringLiteral("backup.zip"), payload, &err), qPrintable(err));

    // Copy the member named like the container into the container's own folder.
    FileOps::CopyItem item;
    item.uri = VfsUri::archiveUriFor(QStringLiteral("file://") + archivePath)
               + QStringLiteral("!/backup.zip");
    item.isDir = false;

    QString error;
    const FileOps::Result result
        = FileOps::copyUrisResult({item}, tmp.path(), false, &error, FileOps::Progress());

    QVERIFY(!result.failed.isEmpty());
    QVERIFY(!error.isEmpty());

    // The archive is still an archive and still holds its member.
    QVector<Entry> entries;
    QVERIFY2(backend.list(root, false, &entries, &err), qPrintable(err));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().name, QStringLiteral("backup.zip"));
}

void TestArchiveCopy::memberExtractsToAnotherFolder()
{
    registerArchiveBackend();

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/bundle.zip");

    const QString payload = tmp.path() + QStringLiteral("/payload.txt");
    writeFile(payload, QByteArray("member contents"));

    ArchiveBackend backend;
    const VfsUri root = VfsUri::parse(QStringLiteral("archive://") + archivePath);
    QString err;
    QVERIFY2(backend.put(root, QStringLiteral("note.txt"), payload, &err), qPrintable(err));

    const QString destination = tmp.path() + QStringLiteral("/out");
    QVERIFY(QDir().mkpath(destination));

    FileOps::CopyItem item;
    item.uri = VfsUri::archiveUriFor(QStringLiteral("file://") + archivePath)
               + QStringLiteral("!/note.txt");
    item.isDir = false;

    QString error;
    const FileOps::Result result
        = FileOps::copyUrisResult({item}, destination, false, &error, FileOps::Progress());
    QVERIFY2(result.failed.isEmpty(), qPrintable(result.error));

    QFile extracted(destination + QStringLiteral("/note.txt"));
    QVERIFY(extracted.open(QIODevice::ReadOnly));
    QCOMPARE(extracted.readAll(), QByteArray("member contents"));
}
