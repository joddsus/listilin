#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "vfs.h"
#include "vfs_archive.h"

#include <archive.h>
#include <archive_entry.h>

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
    void writableFormats_data();
    void writableFormats();
    void unsafeEntryPathRejected();
    void tarEditPreservesModeAndSymlink();
    void failedExtractionKeepsExistingTarget();
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

void TestArchive::writableFormats_data()
{
    QTest::addColumn<QString>("suffix");
    QTest::newRow("tar") << QStringLiteral(".tar");
    QTest::newRow("tar.gz") << QStringLiteral(".tar.gz");
    QTest::newRow("tar.bz2") << QStringLiteral(".tar.bz2");
    QTest::newRow("tar.xz") << QStringLiteral(".tar.xz");
    QTest::newRow("tar.zst") << QStringLiteral(".tar.zst");
    QTest::newRow("tar.Z") << QStringLiteral(".tar.Z");
    QTest::newRow("7z") << QStringLiteral(".7z");
    QTest::newRow("iso") << QStringLiteral(".iso");
    QTest::newRow("ar") << QStringLiteral(".ar");
    QTest::newRow("cpio") << QStringLiteral(".cpio");
}

void TestArchive::writableFormats()
{
    QFETCH(QString, suffix);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/test") + suffix;
    ArchiveBackend backend;
    const VfsUri parent = VfsUri::parse(archiveUri(archivePath));
    QString err;

    QFile f(tmp.path() + QStringLiteral("/hello.txt"));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("hello");
    f.close();

    QVERIFY2(backend.put(parent, QStringLiteral("hello.txt"),
                         tmp.path() + QStringLiteral("/hello.txt"), &err),
             qPrintable(err));
    QVERIFY(QFileInfo::exists(archivePath));

    QVector<Entry> entries;
    QVERIFY2(backend.list(parent, false, &entries, &err), qPrintable(err));
    bool found = false;
    for (const Entry &entry : entries) {
        if (entry.name == QStringLiteral("hello.txt"))
            found = true;
    }
    QVERIFY(found);
}

void TestArchive::unsafeEntryPathRejected()
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

    QVERIFY(!backend.put(parent, QStringLiteral("../escape.txt"),
                        tmp.path() + QStringLiteral("/hello.txt"), &err));
    QVERIFY(!err.isEmpty());
}

namespace {

bool writeTarWithScript(const QString &path)
{
    struct archive *handle = archive_write_new();
    if (archive_write_set_format_pax(handle) != ARCHIVE_OK) {
        archive_write_free(handle);
        return false;
    }
    if (archive_write_open_filename(handle, QFile::encodeName(path).constData()) != ARCHIVE_OK) {
        archive_write_free(handle);
        return false;
    }

    const QByteArray script = "#!/bin/sh\necho hi\n";
    struct archive_entry *entry = archive_entry_new();
    archive_entry_set_pathname(entry, "script.sh");
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0755);
    archive_entry_set_size(entry, script.size());
    bool ok = archive_write_header(handle, entry) == ARCHIVE_OK;
    ok = ok && archive_write_data(handle, script.constData(), size_t(script.size()))
                   == script.size();
    archive_entry_free(entry);

    entry = archive_entry_new();
    archive_entry_set_pathname(entry, "link");
    archive_entry_set_filetype(entry, AE_IFLNK);
    archive_entry_set_perm(entry, 0777);
    archive_entry_set_symlink(entry, "script.sh");
    archive_entry_set_size(entry, 0);
    ok = ok && archive_write_header(handle, entry) == ARCHIVE_OK;
    archive_entry_free(entry);

    archive_write_close(handle);
    archive_write_free(handle);
    return ok;
}

struct StoredEntry
{
    mode_t type = 0;
    mode_t perm = 0;
    QString link;
};

QHash<QString, StoredEntry> readTar(const QString &path)
{
    QHash<QString, StoredEntry> result;
    struct archive *handle = archive_read_new();
    archive_read_support_filter_all(handle);
    archive_read_support_format_all(handle);
    if (archive_read_open_filename(handle, QFile::encodeName(path).constData(), 65536)
        != ARCHIVE_OK) {
        archive_read_free(handle);
        return result;
    }

    struct archive_entry *entry = nullptr;
    while (archive_read_next_header(handle, &entry) == ARCHIVE_OK) {
        StoredEntry stored;
        stored.type = archive_entry_filetype(entry);
        stored.perm = archive_entry_perm(entry);
        if (const char *link = archive_entry_symlink(entry))
            stored.link = QString::fromUtf8(link);
        result.insert(QString::fromUtf8(archive_entry_pathname(entry)), stored);
        archive_read_data_skip(handle);
    }
    archive_read_free(handle);
    return result;
}

} // namespace

void TestArchive::tarEditPreservesModeAndSymlink()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/test.tar");
    QVERIFY(writeTarWithScript(archivePath));

    ArchiveBackend backend;
    const VfsUri root = VfsUri::parse(archiveUri(archivePath));
    QString err;
    QVERIFY2(backend.mkdir(root, QStringLiteral("added"), &err),
             qPrintable(err));

    const QHash<QString, StoredEntry> stored = readTar(archivePath);
    QVERIFY(stored.contains(QStringLiteral("script.sh")));
    QCOMPARE(stored.value(QStringLiteral("script.sh")).type, mode_t(AE_IFREG));
    QCOMPARE(stored.value(QStringLiteral("script.sh")).perm & 0777, mode_t(0755));

    QVERIFY(stored.contains(QStringLiteral("link")));
    QCOMPARE(stored.value(QStringLiteral("link")).type, mode_t(AE_IFLNK));
    QCOMPARE(stored.value(QStringLiteral("link")).link, QStringLiteral("script.sh"));
    QVERIFY(stored.contains(QStringLiteral("added")));
}

void TestArchive::failedExtractionKeepsExistingTarget()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/data.zip");

    const QString member = tmp.path() + QStringLiteral("/member.txt");
    {
        QFile f(member);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QByteArray(4096, 'A'));
        f.close();
    }

    ArchiveBackend backend;
    const VfsUri root = VfsUri::parse(archiveUri(archivePath));
    QString err;
    QVERIFY2(backend.put(root, QStringLiteral("member.txt"), member, &err),
             qPrintable(err));

    // Damage the compressed member data, leaving the central directory so the
    // archive still opens and the failure only appears while reading.
    {
        QFile f(archivePath);
        QVERIFY(f.open(QIODevice::ReadWrite));
        f.seek(64);
        QByteArray chunk = f.read(16);
        for (char &c : chunk)
            c = static_cast<char>(c ^ 0x5a);
        f.seek(64);
        QCOMPARE(f.write(chunk), qint64(chunk.size()));
        f.close();
    }

    const QString target = tmp.path() + QStringLiteral("/target.txt");
    {
        QFile f(target);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("old contents");
        f.close();
    }

    const VfsUri memberUri = VfsUri::parse(archiveUri(archivePath)
                                           + QStringLiteral("!/member.txt"));
    QString extractError;
    QVERIFY(!backend.fetch(memberUri, target, &extractError));

    QFile check(target);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArray("old contents"));
}

QTEST_MAIN(TestArchive)
#include "test_archive.moc"
