#include <QtTest/QtTest>

#include <QFile>
#include <QTemporaryDir>

#include <memory>

#include "temporarycopy.h"
#include "vfs.h"
#include "vfs_archive.h"

class TestTemporaryCopy : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void sameMemberNameGetsItsOwnFile();
};

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(f.write(data), qint64(data.size()));
}

static QByteArray readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

static QString archiveUri(const QString &path)
{
    return VfsUri::archiveUriFor(QStringLiteral("file://") + path);
}

void TestTemporaryCopy::initTestCase()
{
    if (!Vfs::instance().backendFor(QStringLiteral("archive")))
        Vfs::instance().addBackend(std::make_shared<ArchiveBackend>());
}

void TestTemporaryCopy::sameMemberNameGetsItsOwnFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString firstPayload = tmp.path() + QStringLiteral("/first.txt");
    const QString secondPayload = tmp.path() + QStringLiteral("/second.txt");
    writeFile(firstPayload, "first guide");
    writeFile(secondPayload, "second guide");

    const QString firstArchive = archiveUri(tmp.path() + QStringLiteral("/first.zip"));
    const QString secondArchive = archiveUri(tmp.path() + QStringLiteral("/second.zip"));
    QString error;
    QVERIFY2(Vfs::instance().put(firstArchive, QStringLiteral("guide.txt"), firstPayload, &error),
             qPrintable(error));
    QVERIFY2(Vfs::instance().put(secondArchive, QStringLiteral("guide.txt"), secondPayload, &error),
             qPrintable(error));

    // Two documents with the same name must not share a local path: opening the
    // second would otherwise change what the first external program reads.
    const QString first = TemporaryCopy::ofUri(
        VfsUri::childUri(firstArchive, QStringLiteral("guide.txt")), &error);
    QVERIFY2(!first.isEmpty(), qPrintable(error));
    const QString second = TemporaryCopy::ofUri(
        VfsUri::childUri(secondArchive, QStringLiteral("guide.txt")), &error);
    QVERIFY2(!second.isEmpty(), qPrintable(error));

    QVERIFY(first != second);
    QCOMPARE(readFile(first), QByteArray("first guide"));
    QCOMPARE(readFile(second), QByteArray("second guide"));
    // Re-reading the first path still gives the first document.
    QCOMPARE(readFile(first), QByteArray("first guide"));
}

QTEST_MAIN(TestTemporaryCopy)
#include "test_temporarycopy.moc"
