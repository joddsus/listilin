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
    void parseRemoteWithPort();
    void parseRemoteWithPassword();
    void parseExplicitFtps();
    void parentUriRemote();
    void childUriRemote();
    void toStringRoundTrip();
    void childUri();
    void parentUri();
    void looksLikeArchive();
    void archiveUriFor();
    void leafName();
    void emptyParse();
    void parentOfRoot();
    void childOfRoot();
    void toLocalPath();
    void predicates();
    void archiveUriForNonLocal();
    void redactCredentials();
    void trailingSpaceIsPartOfTheName();
    void specialCharactersRoundTrip();
    void unicodeRoundTrip();
    void exclamationMarkInRemotePath();
    void exclamationMarkInArchiveFileName();
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

void TestVfsUri::parseRemoteWithPort()
{
    const VfsUri uri = VfsUri::parse(QStringLiteral("sftp://user@host:2222/tmp"));
    QCOMPARE(uri.scheme, QStringLiteral("sftp"));
    QCOMPARE(uri.authority, QStringLiteral("user@host:2222"));
    QCOMPARE(uri.path, QStringLiteral("/tmp"));
    QCOMPARE(uri.toString(), QStringLiteral("sftp://user@host:2222/tmp"));
}

void TestVfsUri::parseRemoteWithPassword()
{
    const VfsUri uri = VfsUri::parse(QStringLiteral("ftp://user:secret@host/pub"));
    QCOMPARE(uri.scheme, QStringLiteral("ftp"));
    QCOMPARE(uri.authority, QStringLiteral("user:secret@host"));
    QCOMPARE(uri.path, QStringLiteral("/pub"));
    QCOMPARE(uri.toString(), QStringLiteral("ftp://user:secret@host/pub"));
}

void TestVfsUri::parseExplicitFtps()
{
    // Explicit FTPS keeps its own scheme in the namespace; only the transport
    // layer rewrites it to ftp + AUTH TLS.
    const VfsUri uri = VfsUri::parse(QStringLiteral("ftpes://user@host:21/pub"));
    QCOMPARE(uri.scheme, QStringLiteral("ftpes"));
    QCOMPARE(uri.authority, QStringLiteral("user@host:21"));
    QCOMPARE(uri.path, QStringLiteral("/pub"));
    QVERIFY(uri.isRemote());
    QCOMPARE(uri.toString(), QStringLiteral("ftpes://user@host:21/pub"));
    QCOMPARE(VfsUri::childUri(uri.toString(), QStringLiteral("a")),
             QStringLiteral("ftpes://user@host:21/pub/a"));
    QCOMPARE(VfsUri::parentUri(QStringLiteral("ftpes://host/a/b")),
             QStringLiteral("ftpes://host/a"));
}

void TestVfsUri::parentUriRemote()
{
    QCOMPARE(VfsUri::parentUri(QStringLiteral("ftp://host/a/b")),
             QStringLiteral("ftp://host/a"));
    QCOMPARE(VfsUri::parentUri(QStringLiteral("ftp://host/a")),
             QStringLiteral("ftp://host/"));
}

void TestVfsUri::childUriRemote()
{
    QCOMPARE(VfsUri::childUri(QStringLiteral("ftp://host/a"), QStringLiteral("b")),
             QStringLiteral("ftp://host/a/b"));
}

void TestVfsUri::toStringRoundTrip()
{
    const QStringList inputs = {
        QStringLiteral("file:///tmp/x"),
        QStringLiteral("archive:///home/a.zip!/src"),
        QStringLiteral("sftp://user@host/var/www"),
        QStringLiteral("ftpes://user@host/pub"),
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

void TestVfsUri::emptyParse()
{
    const VfsUri uri = VfsUri::parse(QString());
    QVERIFY(!uri.isValid());
    QVERIFY(uri.scheme.isEmpty());
}

void TestVfsUri::parentOfRoot()
{
    // Parent of root stays at root (no crash, no empty path).
    QCOMPARE(VfsUri::parentUri(QStringLiteral("file:///")), QStringLiteral("file:///"));
    QCOMPARE(VfsUri::parentUri(QStringLiteral("archive:///a.zip")), QStringLiteral("file:///"));
}

void TestVfsUri::childOfRoot()
{
    QCOMPARE(VfsUri::childUri(QStringLiteral("file:///"), QStringLiteral("tmp")),
             QStringLiteral("file:///tmp"));
    QCOMPARE(VfsUri::childUri(QStringLiteral("archive:///a.zip"), QStringLiteral("src")),
             QStringLiteral("archive:///a.zip!/src"));
}

void TestVfsUri::toLocalPath()
{
    QCOMPARE(VfsUri::parse(QStringLiteral("file:///tmp/x")).toLocalPath(), QStringLiteral("/tmp/x"));
    QVERIFY(VfsUri::parse(QStringLiteral("ftp://host/x")).toLocalPath().isEmpty());
    QVERIFY(VfsUri::parse(QStringLiteral("archive:///a.zip!/x")).toLocalPath().isEmpty());
}

void TestVfsUri::predicates()
{
    QVERIFY(VfsUri::parse(QStringLiteral("file:///x")).isLocal());
    QVERIFY(!VfsUri::parse(QStringLiteral("file:///x")).isArchive());
    QVERIFY(!VfsUri::parse(QStringLiteral("file:///x")).isRemote());
    QVERIFY(VfsUri::parse(QStringLiteral("archive:///a.zip")).isArchive());
    QVERIFY(VfsUri::parse(QStringLiteral("ftp://h/x")).isRemote());
    QVERIFY(VfsUri::parse(QStringLiteral("smb://h/x")).isRemote());
}

void TestVfsUri::archiveUriForNonLocal()
{
    // archiveUriFor on a non-local URI still produces an archive scheme.
    const QString result = VfsUri::archiveUriFor(QStringLiteral("ftp://host/a.zip"));
    QVERIFY(result.startsWith(QStringLiteral("archive://")));
}

void TestVfsUri::redactCredentials()
{
    QCOMPARE(VfsUri::redactCredentials(QStringLiteral("ftp://user:secret@host/pub")),
             QStringLiteral("ftp://user@host/pub"));
    QCOMPARE(VfsUri::redactCredentials(QStringLiteral("file:///tmp/x")),
             QStringLiteral("file:///tmp/x"));
}

void TestVfsUri::trailingSpaceIsPartOfTheName()
{
    // "note.txt" and "note.txt " are two different files. Trimming the URI made
    // Delete act on the neighbour without a space.
    const QString spaced = QStringLiteral("file:///tmp/note.txt ");
    const VfsUri uri = VfsUri::parse(spaced);
    QCOMPARE(uri.path, QStringLiteral("/tmp/note.txt "));
    QVERIFY(uri.path != VfsUri::parse(QStringLiteral("file:///tmp/note.txt")).path);
}

void TestVfsUri::specialCharactersRoundTrip()
{
    const QStringList names = {QStringLiteral("/tmp/a#b.txt"),
                               QStringLiteral("/tmp/100%.txt"),
                               QStringLiteral("/tmp/a?b&c.txt")};
    for (const QString &path : names) {
        const VfsUri uri = VfsUri::parse(QStringLiteral("file://") + path);
        QCOMPARE(uri.path, path);
        QCOMPARE(VfsUri::parse(uri.toString()).path, path);
    }
}

void TestVfsUri::unicodeRoundTrip()
{
    const QString path = QStringLiteral("/tmp/blåbær/ÆØÅ.txt");
    const VfsUri uri = VfsUri::parse(QStringLiteral("file://") + path);
    QCOMPARE(uri.path, path);
    QCOMPARE(VfsUri::parse(uri.toString()).path, path);
}

void TestVfsUri::exclamationMarkInRemotePath()
{
    for (const QString &scheme : {QStringLiteral("sftp"), QStringLiteral("ftp"),
                                  QStringLiteral("smb")}) {
        const QString text = scheme + QStringLiteral("://user@host/Festival!/photos");
        const VfsUri uri = VfsUri::parse(text);
        QCOMPARE(uri.scheme, scheme);
        QCOMPARE(uri.path, QStringLiteral("/Festival!/photos"));
        QVERIFY(uri.isRemote());
        QCOMPARE(uri.toString(), text);
    }
}

void TestVfsUri::exclamationMarkInArchiveFileName()
{
    // The archive file may contain '!'; the last "!/" is the member separator,
    // so the file name is not truncated at its own exclamation mark.
    const VfsUri uri = VfsUri::parse(QStringLiteral("archive:///tmp/Festival!.zip!/guide.txt"));
    QCOMPARE(uri.scheme, QStringLiteral("archive"));
    QCOMPARE(uri.path, QStringLiteral("/tmp/Festival!.zip"));
    QCOMPARE(uri.inner, QStringLiteral("/guide.txt"));
    QCOMPARE(uri.toString(), QStringLiteral("archive:///tmp/Festival!.zip!/guide.txt"));
}

QTEST_MAIN(TestVfsUri)
#include "test_vfsuri.moc"
