#include "fileshred.h"

#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

class TestFileShred : public QObject
{
    Q_OBJECT

private slots:
    void shredsFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("secret.txt"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("sensitive data");
        f.close();

        QString error;
        QVERIFY(FileShred::shredFile(path, 2, &error));
        QVERIFY(error.isEmpty());
        QVERIFY(!QFile::exists(path));
    }

    void missingFileFails()
    {
        QString error;
        QVERIFY(!FileShred::shredFile(QStringLiteral("/no/such/file"), 1, &error));
        QVERIFY(!error.isEmpty());
    }

    void shredsTree()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = dir.filePath(QStringLiteral("tree"));
        QVERIFY(QDir().mkpath(root + QStringLiteral("/sub")));
        QFile f(root + QStringLiteral("/sub/a.txt"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("data");
        f.close();

        QString error;
        QVERIFY(FileShred::shredTree(root, 1, &error));
        QVERIFY(error.isEmpty());
        QVERIFY(!QDir(root).exists());
    }
};

QTEST_MAIN(TestFileShred)
#include "test_fileshred.moc"