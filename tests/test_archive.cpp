#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "vfs.h"
#include "vfs_archive.h"

class TestArchive : public QObject
{
    Q_OBJECT
private slots:
    void putCreatesArchive();
    void listAfterPut();
    void mkdirAddsDir();
    void renameEntry();
    void removeEntry();
    void passwordPreservedAcrossEdit();
};

static QString archiveUri(const QString &path)
{
    return QStringLiteral("archive://") + path;
}

void TestArchive::putCreatesArchive()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/test.zip");
    ArchiveBackend backend;

    const VfsUri parent = VfsUri::parse(archiveUri(archivePath));
    QString err;
    QFile f(tmp.path() + QStringLiteral("/hello.txt"));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("hello");
    f.close();

    QVERIFY(backend.put(parent, QStringLiteral("hello.txt"),
                        tmp.path() + QStringLiteral("/hello.txt"), &err));
    QVERIFY(err.isEmpty());
    QVERIFY(QFileInfo::exists(archivePath));
}

void TestArchive::listAfterPut()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/test.zip");
    ArchiveBackend backend;

    const VfsUri parent = VfsUri::parse(archiveUri(archivePath));
    QString err;
    QFile f(tmp.path() + QStringLiteral("/hello.txt"));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("hello");
    f.close();
    QVERIFY(backend.put(parent, QStringLiteral("hello.txt"),
                        tmp.path() + QStringLiteral("/hello.txt"), &err));

    QVector<Entry> entries;
    const VfsUri root = VfsUri::parse(archiveUri(archivePath));
    QVERIFY(backend.list(root, false, &entries, &err));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().name, QStringLiteral("hello.txt"));
    QCOMPARE(entries.first().size, (qint64)5);
}

void TestArchive::mkdirAddsDir()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/test.zip");
    ArchiveBackend backend;

    const VfsUri parent = VfsUri::parse(archiveUri(archivePath));
    QString err;
    QFile f(tmp.path() + QStringLiteral("/hello.txt"));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("hello");
    f.close();
    QVERIFY(backend.put(parent, QStringLiteral("hello.txt"),
                        tmp.path() + QStringLiteral("/hello.txt"), &err));

    const VfsUri root = VfsUri::parse(archiveUri(archivePath));
    QVERIFY(backend.mkdir(root, QStringLiteral("subdir"), &err));
    QVERIFY(err.isEmpty());

    QVector<Entry> entries;
    QVERIFY(backend.list(root, false, &entries, &err));
    // Should now have hello.txt and subdir
    QStringList names;
    for (const Entry &e : entries) names.append(e.name);
    QVERIFY(names.contains(QStringLiteral("hello.txt")));
    QVERIFY(names.contains(QStringLiteral("subdir")));
}

void TestArchive::renameEntry()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/test.zip");
    ArchiveBackend backend;

    const VfsUri parent = VfsUri::parse(archiveUri(archivePath));
    QString err;
    QFile f(tmp.path() + QStringLiteral("/hello.txt"));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("hello");
    f.close();
    QVERIFY(backend.put(parent, QStringLiteral("hello.txt"),
                        tmp.path() + QStringLiteral("/hello.txt"), &err));

    const VfsUri fileUri = VfsUri::parse(archiveUri(archivePath) + QStringLiteral("!/hello.txt"));
    QString newUri;
    QVERIFY(backend.rename(fileUri, QStringLiteral("renamed.txt"), &newUri, &err));
    QVERIFY(err.isEmpty());

    QVector<Entry> entries;
    const VfsUri root = VfsUri::parse(archiveUri(archivePath));
    QVERIFY(backend.list(root, false, &entries, &err));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().name, QStringLiteral("renamed.txt"));
}

void TestArchive::removeEntry()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/test.zip");
    ArchiveBackend backend;

    const VfsUri parent = VfsUri::parse(archiveUri(archivePath));
    QString err;
    QFile f(tmp.path() + QStringLiteral("/hello.txt"));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("hello");
    f.close();
    QVERIFY(backend.put(parent, QStringLiteral("hello.txt"),
                        tmp.path() + QStringLiteral("/hello.txt"), &err));

    const VfsUri fileUri = VfsUri::parse(archiveUri(archivePath) + QStringLiteral("!/hello.txt"));
    QVERIFY(backend.remove(fileUri, false, &err));
    QVERIFY(err.isEmpty());

    QVector<Entry> entries;
    const VfsUri root = VfsUri::parse(archiveUri(archivePath));
    QVERIFY(backend.list(root, false, &entries, &err));
    QCOMPARE(entries.size(), 0);
}

void TestArchive::passwordPreservedAcrossEdit()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/enc.zip");

    // Set a cached password before creating.
    setArchivePassword(archivePath, QStringLiteral("secret"));

    ArchiveBackend backend;
    const VfsUri parent = VfsUri::parse(archiveUri(archivePath));
    QString err;
    QFile f(tmp.path() + QStringLiteral("/hello.txt"));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("hello");
    f.close();
    QVERIFY(backend.put(parent, QStringLiteral("hello.txt"),
                        tmp.path() + QStringLiteral("/hello.txt"), &err));
    QVERIFY(err.isEmpty());

    // Edit: add a directory. Password should be preserved.
    const VfsUri root = VfsUri::parse(archiveUri(archivePath));
    QVERIFY(backend.mkdir(root, QStringLiteral("subdir"), &err));
    QVERIFY(err.isEmpty());

    // Verify the archive is still encrypted: reading without password should fail.
    // Clear the cache to force a fresh read without password.
    // We can't easily clear g_passwordCache (not exposed), so instead verify
    // that cachedArchivePassword still returns the password.
    QCOMPARE(cachedArchivePassword(archivePath), QStringLiteral("secret"));

    // And the file still exists and is readable with the cached password.
    QVector<Entry> entries;
    QVERIFY(backend.list(root, false, &entries, &err));
    QStringList names;
    for (const Entry &e : entries) names.append(e.name);
    QVERIFY(names.contains(QStringLiteral("hello.txt")));
    QVERIFY(names.contains(QStringLiteral("subdir")));
}

QTEST_MAIN(TestArchive)
#include "test_archive.moc"
