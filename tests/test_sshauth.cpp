#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "sshauth.h"

class TestSshAuth : public QObject
{
    Q_OBJECT
private slots:
    void findsFirstDefaultIdentity();
    void missingDirectoryHasNoIdentity();
    void derivesPublicKeyOnlyWhenPresent();
    void validatesConfiguredFiles();
};

void TestSshAuth::findsFirstDefaultIdentity()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString sshDir = dir.path();

    // The order matters: ed25519 wins even when an RSA key also exists.
    QFile rsa(sshDir + QStringLiteral("/id_rsa"));
    QVERIFY(rsa.open(QIODevice::WriteOnly));
    rsa.write("rsa");
    rsa.close();
    QFile ed(sshDir + QStringLiteral("/id_ed25519"));
    QVERIFY(ed.open(QIODevice::WriteOnly));
    ed.write("ed");
    ed.close();

    QCOMPARE(SshAuth::findDefaultIdentity(sshDir),
             QDir(sshDir).absoluteFilePath(QStringLiteral("id_ed25519")));
}

void TestSshAuth::missingDirectoryHasNoIdentity()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(SshAuth::findDefaultIdentity(dir.path() + QStringLiteral("/absent")).isEmpty());
}

void TestSshAuth::derivesPublicKeyOnlyWhenPresent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString key = dir.path() + QStringLiteral("/id_ed25519");
    QFile privateKey(key);
    QVERIFY(privateKey.open(QIODevice::WriteOnly));
    privateKey.write("private");
    privateKey.close();

    QVERIFY(SshAuth::publicKeyFor(key).isEmpty());

    QFile publicKey(key + QStringLiteral(".pub"));
    QVERIFY(publicKey.open(QIODevice::WriteOnly));
    publicKey.write("ssh-ed25519 AAAA test");
    publicKey.close();
    QCOMPARE(SshAuth::publicKeyFor(key), key + QStringLiteral(".pub"));
}

void TestSshAuth::validatesConfiguredFiles()
{
    QString error;
    QVERIFY(SshAuth::validate(SshAuth::Options(), &error));
    QVERIFY(error.isEmpty());

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString key = dir.path() + QStringLiteral("/id_ed25519");
    const QString known = dir.path() + QStringLiteral("/known_hosts");
    for (const QString &path : {key, known}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("data");
        file.close();
    }

    SshAuth::Options options;
    options.identityFile = key;
    options.publicKeyFile = key + QStringLiteral(".pub");
    options.knownHostsFile = known;
    // The public key file was not created in this test, so validation must say
    // so rather than silently ignoring the field.
    QVERIFY(!SshAuth::validate(options, &error));
    QVERIFY(error.contains(QStringLiteral("public key")));

    options.publicKeyFile.clear();
    QVERIFY(SshAuth::validate(options, &error));
    QVERIFY(error.isEmpty());

    options.identityFile = dir.path();
    QVERIFY(!SshAuth::validate(options, &error));
    QVERIFY(error.contains(QStringLiteral("not a file")));

    options.identityFile = key;
    options.knownHostsFile = dir.path() + QStringLiteral("/missing");
    QVERIFY(!SshAuth::validate(options, &error));
    QVERIFY(error.contains(QStringLiteral("does not exist")));
}

QTEST_MAIN(TestSshAuth)
#include "test_sshauth.moc"
