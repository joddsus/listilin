#include <QtTest/QtTest>

#include "vfs.h"

class TestVfsUri : public QObject
{
    Q_OBJECT
private slots:
    void parseBarePath();
    void parseFile();
    void parseArchive();
    void parseRemote();
    void toStringRoundTrip();
    void childUri();
    void parentUri();
    void looksLikeArchive();
    void archiveUriFor();
    void leafName();
};

void TestVfsUri::parseBarePath()
{
    const VfsUri uri = VfsUri::parse(QStringLiteral("/home/ola"));
    QCOMPARE(uri.scheme, QStringLiteral("file"));
    QCOMPARE(uri.path, QStringLiteral("/home/ola"));
    QVERIFY(uri.isValid());
    QVERIFY(uri.isLocal());
}

void TestVfsUri::parseFile()
{
    const VfsUri uri = VfsUri::parse(QStringLiteral("file:///tmp/x"));
    QCOMPARE(uri.scheme, QStringLiteral("file"));
    QCOMPARE(uri.path, QStringLiteral("/tmp/x"));
    QCOMPARE(uri.toString(), QStringLiteral("file:///tmp/x"));
}

void TestVfsUri::parseArchive()
{
    const VfsUri uri = VfsUri::parse(QStringLiteral("archive:///home/a.zip!/src"));
    QCOMPARE(uri.scheme, QStringLiteral("archive"));
    QCOMPARE(uri.path, QStringLiteral("/home/a.zip"));
    QCOMPARE(uri.inner, QStringLiteral("/src"));
    QVERIFY(uri.isArchive());
}

void TestVfsUri::parseRemote()
{
    const VfsUri uri = VfsUri::parse(QStringLiteral("ftp://user@host/pub"));
    QCOMPARE(uri.scheme, QStringLiteral("ftp"));
    QCOMPARE(uri.authority, QStringLiteral("user@host"));
    QCOMPARE(uri.path, QStringLiteral("/pub"));
    QVERIFY(uri.isRemote());
}

void TestVfsUri::toStringRoundTrip()
{
    const QStringList inputs = {
        QStringLiteral("file:///tmp/x"),
        QStringLiteral("archive:///home/a.zip!/src"),
        QStringLiteral("sftp://user@host/var/www"),
        QStringLiteral("smb://host/share"),
    };
    for (const QString &input : inputs) {
        const VfsUri uri = VfsUri::parse(input);
        QCOMPARE(uri.toString(), input);
    }
}

void TestVfsUri::childUri()
{
    QCOMPARE(VfsUri::childUri(QStringLiteral("file:///tmp"), QStringLiteral("foo")),
             QStringLiteral("file:///tmp/foo"));
    QCOMPARE(VfsUri::childUri(QStringLiteral("archive:///a.zip!/src"), QStringLiteral("main.cpp")),
             QStringLiteral("archive:///a.zip!/src/main.cpp"));
}

void TestVfsUri::parentUri()
{
    QCOMPARE(VfsUri::parentUri(QStringLiteral("file:///tmp/foo")),
             QStringLiteral("file:///tmp"));
    QCOMPARE(VfsUri::parentUri(QStringLiteral("archive:///a.zip!/src/main.cpp")),
             QStringLiteral("archive:///a.zip!/src"));
}

void TestVfsUri::looksLikeArchive()
{
    QVERIFY(VfsUri::looksLikeArchive(QStringLiteral("file:///tmp/a.zip")));
    QVERIFY(VfsUri::looksLikeArchive(QStringLiteral("file:///tmp/a.tar.gz")));
    QVERIFY(!VfsUri::looksLikeArchive(QStringLiteral("file:///tmp/a.txt")));
    QVERIFY(!VfsUri::looksLikeArchive(QStringLiteral("ftp://host/a.zip")));
}

void TestVfsUri::archiveUriFor()
{
    QCOMPARE(VfsUri::archiveUriFor(QStringLiteral("file:///tmp/a.zip")),
             QStringLiteral("archive:///tmp/a.zip"));
}

void TestVfsUri::leafName()
{
    QCOMPARE(vfsLeafName(QStringLiteral("file:///tmp/foo.txt")), QStringLiteral("foo.txt"));
    QCOMPARE(vfsLeafName(QStringLiteral("archive:///a.zip!/src/main.cpp")), QStringLiteral("main.cpp"));
    QCOMPARE(vfsLeafName(QStringLiteral("file:///")), QStringLiteral("/"));
}

QTEST_MAIN(TestVfsUri)
#include "test_vfsuri.moc"
