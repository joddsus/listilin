#include <QtTest/QtTest>

#include "remoteuri.h"

class TestRemoteUri : public QObject
{
    Q_OBJECT
private slots:
    void schemes();
    void hosts();
    void ports();
    void paths();
    void build();
};

void TestRemoteUri::schemes()
{
    QVERIFY(RemoteUri::isSupportedScheme(QStringLiteral("ftp")));
    QVERIFY(RemoteUri::isSupportedScheme(QStringLiteral("SFTP")));
    QVERIFY(!RemoteUri::isSupportedScheme(QStringLiteral("http")));
}

void TestRemoteUri::hosts()
{
    QVERIFY(RemoteUri::isValidHost(QStringLiteral("example.com")));
    QVERIFY(RemoteUri::isValidHost(QStringLiteral("127.0.0.1")));
    QVERIFY(!RemoteUri::isValidHost(QString()));
    QVERIFY(!RemoteUri::isValidHost(QStringLiteral("a/b")));
    QVERIFY(!RemoteUri::isValidHost(QStringLiteral("a@b")));
}

void TestRemoteUri::ports()
{
    QVERIFY(RemoteUri::isValidPort(QString()));
    QVERIFY(RemoteUri::isValidPort(QStringLiteral("21")));
    QVERIFY(!RemoteUri::isValidPort(QStringLiteral("0")));
    QVERIFY(!RemoteUri::isValidPort(QStringLiteral("70000")));
    QVERIFY(!RemoteUri::isValidPort(QStringLiteral("abc")));
    int value = 0;
    QVERIFY(RemoteUri::isValidPort(QStringLiteral("22"), &value));
    QCOMPARE(value, 22);
}

void TestRemoteUri::paths()
{
    QVERIFY(RemoteUri::isSafePath(QString()));
    QVERIFY(RemoteUri::isSafePath(QStringLiteral("/a/b")));
    QVERIFY(!RemoteUri::isSafePath(QStringLiteral("../x")));
    QVERIFY(!RemoteUri::isSafePath(QStringLiteral("relative")));
    QCOMPARE(RemoteUri::normalizePath(QStringLiteral("a/b/")), QStringLiteral("/a/b"));
}

void TestRemoteUri::build()
{
    QString error;
    const QString uri = RemoteUri::build(QStringLiteral("sftp"), QStringLiteral("host"),
                                         QStringLiteral("22"), QStringLiteral("user"),
                                         QStringLiteral("pass"), QStringLiteral("/tmp"), &error);
    QCOMPARE(uri, QStringLiteral("sftp://user:pass@host:22/tmp"));
    QVERIFY(error.isEmpty());

    QVERIFY(RemoteUri::build(QStringLiteral("http"), QStringLiteral("host"), QString(),
                             QString(), QString(), QStringLiteral("/"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(TestRemoteUri)
#include "test_remoteuri.moc"