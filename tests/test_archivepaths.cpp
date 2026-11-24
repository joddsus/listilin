#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "archivepaths.h"

class TestArchivePaths : public QObject
{
    Q_OBJECT
private slots:
    void cleanNames();
    void validPaths();
    void detectFormats();
    void writablePaths();
    void splitPartsEmpty();
};

void TestArchivePaths::cleanNames()
{
    QCOMPARE(ArchivePaths::cleanEntryName(QStringLiteral("a/b/c.txt")), QStringLiteral("a/b/c.txt"));
    QVERIFY(ArchivePaths::cleanEntryName(QStringLiteral("../escape.txt")).isEmpty());
    QVERIFY(ArchivePaths::cleanEntryName(QStringLiteral("a\\b/c")).isEmpty() == false);
}

void TestArchivePaths::validPaths()
{
    QVERIFY(ArchivePaths::isValidEntryPath(QStringLiteral("a/b/c.txt")));
    QVERIFY(!ArchivePaths::isValidEntryPath(QString()));
    QVERIFY(!ArchivePaths::isValidEntryPath(QStringLiteral("../x")));
    QVERIFY(!ArchivePaths::isValidEntryPath(QStringLiteral("C:/x")));
}

void TestArchivePaths::detectFormats()
{
    QString filter;
    QCOMPARE(ArchivePaths::detectWriteFormat(QStringLiteral("a.zip"), &filter), QStringLiteral("zip"));
    QCOMPARE(filter, QStringLiteral("none"));
    QCOMPARE(ArchivePaths::detectWriteFormat(QStringLiteral("a.tar.gz"), &filter), QStringLiteral("pax"));
    QCOMPARE(filter, QStringLiteral("gzip"));
    QVERIFY(ArchivePaths::detectWriteFormat(QStringLiteral("a.arj"), nullptr).isEmpty());
    QVERIFY(ArchivePaths::detectWriteFormat(QStringLiteral("a.cab"), nullptr).isEmpty());
    QVERIFY(ArchivePaths::detectWriteFormat(QStringLiteral("a.lha"), nullptr).isEmpty());
    QVERIFY(ArchivePaths::detectWriteFormat(QStringLiteral("a.rar"), nullptr).isEmpty());
    QVERIFY(ArchivePaths::detectWriteFormat(QStringLiteral("a.rar5"), nullptr).isEmpty());
}

void TestArchivePaths::writablePaths()
{
    QVERIFY(ArchivePaths::isWritablePath(QStringLiteral("a.zip")));
    QVERIFY(ArchivePaths::isWritablePath(QStringLiteral("a.tar")));
    QVERIFY(ArchivePaths::isWritablePath(QStringLiteral("archive")));
    QVERIFY(!ArchivePaths::isWritablePath(QStringLiteral("a.arj")));
    QVERIFY(!ArchivePaths::isWritablePath(QStringLiteral("a.cab")));
}

void TestArchivePaths::splitPartsEmpty()
{
    QVERIFY(ArchivePaths::splitParts(QStringLiteral("/tmp/does-not-exist.zip")).isEmpty());
}

QTEST_MAIN(TestArchivePaths)
#include "test_archivepaths.moc"