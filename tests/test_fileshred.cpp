#include "fileshred.h"

#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

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

    void shredsSymlinkNotItsTarget()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString target = dir.filePath(QStringLiteral("real.txt"));
        QFile f(target);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("must stay");
        f.close();

        const QString link = dir.filePath(QStringLiteral("link.txt"));
        QVERIFY(QFile::link(target, link));

        QString error;
        QVERIFY(FileShred::shredFile(link, 2, &error));
        QVERIFY(!QFileInfo(link).isSymLink());

        QFile check(target);
        QVERIFY(check.open(QIODevice::ReadOnly));
        QCOMPARE(check.readAll(), QByteArray("must stay"));
    }

    void shredsFolderSymlinkNotItsTarget()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString realDir = dir.filePath(QStringLiteral("real"));
        QVERIFY(QDir().mkpath(realDir));
        QFile f(realDir + QStringLiteral("/inside.txt"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("must stay");
        f.close();

        const QString link = dir.filePath(QStringLiteral("link"));
        QVERIFY(QFile::link(realDir, link));

        QString error;
        QVERIFY(FileShred::shredTree(link, 1, &error));
        QVERIFY(!QFileInfo(link).isSymLink());
        QVERIFY(QFileInfo::exists(realDir + QStringLiteral("/inside.txt")));
    }

    void shredDoesNotOverwriteOtherHardLinks()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = dir.filePath(QStringLiteral("first.txt"));
        QFile f(first);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("shared contents");
        f.close();

        const QString second = dir.filePath(QStringLiteral("second.txt"));
#ifdef Q_OS_UNIX
        QCOMPARE(::link(QFile::encodeName(first).constData(),
                        QFile::encodeName(second).constData()),
                 0);
#else
        QSKIP("Hard links are not available on this platform");
#endif
        QCOMPARE(QFileInfo(first).size(), QFileInfo(second).size());

        QString error;
        QVERIFY(FileShred::shredFile(first, 2, &error));
        QVERIFY(!QFileInfo::exists(first));

        QFile check(second);
        QVERIFY(check.open(QIODevice::ReadOnly));
        QCOMPARE(check.readAll(), QByteArray("shared contents"));
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