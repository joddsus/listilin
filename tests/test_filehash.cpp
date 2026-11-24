#include "filehash.h"

#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

class TestFileHash : public QObject
{
    Q_OBJECT

private slots:
    void emptyFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("empty.bin"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        QString error;
        const QString digest = FileHash::hexDigest(path, FileHash::Algorithm::Sha256, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(digest, QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    }

    void knownContent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("data.txt"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("abc");
        f.close();

        QCOMPARE(FileHash::hexDigest(path, FileHash::Algorithm::Md5), QStringLiteral("900150983cd24fb0d6963f7d28e17f72"));
        QCOMPARE(FileHash::hexDigest(path, FileHash::Algorithm::Sha1), QStringLiteral("a9993e364706816aba3e25717850c26c9cd0d89d"));
        QCOMPARE(FileHash::hexDigest(path, FileHash::Algorithm::Sha256),
                 QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    }

    void missingFile()
    {
        QString error;
        const QString digest = FileHash::hexDigest(QStringLiteral("/no/such/file"), FileHash::Algorithm::Sha256, &error);
        QVERIFY(digest.isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void algorithmNames()
    {
        QCOMPARE(FileHash::algorithmName(FileHash::Algorithm::Md5), QStringLiteral("MD5"));
        QCOMPARE(FileHash::supportedAlgorithms().size(), 3);
    }
};

QTEST_MAIN(TestFileHash)
#include "test_filehash.moc"