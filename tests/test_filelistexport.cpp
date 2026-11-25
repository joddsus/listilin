#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>

#include "filelistexport.h"

class TestFileListExport : public QObject
{
    Q_OBJECT
private slots:
    void quotesAndProtectsFormulaCells();
    void writesHeaderAndUtcTime();
    void refusesToReplaceExistingFile();
};

void TestFileListExport::quotesAndProtectsFormulaCells()
{
    QCOMPARE(FileListExport::textCell(QStringLiteral("plain")), QStringLiteral("\"plain\""));
    QCOMPARE(FileListExport::textCell(QStringLiteral("a\"b")), QStringLiteral("\"a\"\"b\""));
    QCOMPARE(FileListExport::textCell(QStringLiteral("=SUM(A1)")), QStringLiteral("\"'=SUM(A1)\""));
    QCOMPARE(FileListExport::textCell(QStringLiteral("  +cmd")), QStringLiteral("\"'  +cmd\""));
    QCOMPARE(FileListExport::textCell(QStringLiteral("\tname")), QStringLiteral("\"'\tname\""));
}

void TestFileListExport::writesHeaderAndUtcTime()
{
    FileListExport::Row row;
    row.name = QStringLiteral("a.txt");
    row.fullPath = QStringLiteral("/tmp/a.txt");
    row.type = QStringLiteral("file");
    row.size = QStringLiteral("12");
    row.modified = FileListExport::modifiedUtc(
        QDateTime(QDate(2026, 11, 25), QTime(12, 0), Qt::UTC));

    const QString csv = FileListExport::toCsv({row});
    QVERIFY(csv.startsWith(QStringLiteral("Name,Full path,Type,Size (bytes),Modified (UTC)\r\n")));
    QVERIFY(csv.contains(QStringLiteral("\"a.txt\",\"/tmp/a.txt\",file,12,2026-11-25T12:00:00Z\r\n")));
}

void TestFileListExport::refusesToReplaceExistingFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/list.csv");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("keep");
    file.close();

    QString error;
    QVERIFY(!FileListExport::writeNew(path, {}, &error));
    QVERIFY(!error.isEmpty());

    QFile check(path);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArray("keep"));
}

QTEST_MAIN(TestFileListExport)
#include "test_filelistexport.moc"
