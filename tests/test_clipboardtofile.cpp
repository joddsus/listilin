#include <QtTest/QtTest>
#include <QImage>

#include "clipboardtofile.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

class TestClipboardToFile : public QObject
{
    Q_OBJECT
private slots:
    void suggestedNameImage();
    void suggestedNameText();
    void uniquePathAvoidsCollision();
    void saveTextWritesFile();
    void saveImageWritesPng();
};

void TestClipboardToFile::suggestedNameImage()
{
    QCOMPARE(ClipboardToFile::suggestedName(true, false, QString()),
             QStringLiteral("clipboard.png"));
}

void TestClipboardToFile::suggestedNameText()
{
    QCOMPARE(ClipboardToFile::suggestedName(false, true, QStringLiteral("hello world")),
             QStringLiteral("hello world.txt"));
}

void TestClipboardToFile::uniquePathAvoidsCollision()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString first = dir.filePath(QStringLiteral("a.txt"));
    QFile f(first);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();

    const QString candidate = ClipboardToFile::uniquePath(dir.path(), QStringLiteral("a.txt"));
    QVERIFY(candidate != first);
    QVERIFY(candidate.endsWith(QStringLiteral("a (2).txt")));
}

void TestClipboardToFile::saveTextWritesFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString written;
    QString error;
    QVERIFY(ClipboardToFile::saveText(dir.path(), QStringLiteral("hello"), &written, &error));
    QVERIFY(QFile::exists(written));
    QFile f(written);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(f.readAll()), QStringLiteral("hello"));
}

void TestClipboardToFile::saveImageWritesPng()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QImage image(4, 4, QImage::Format_RGB32);
    image.fill(Qt::red);
    QString written;
    QString error;
    QVERIFY(ClipboardToFile::saveImage(dir.path(), image, &written, &error));
    QVERIFY(QFile::exists(written));
    QImage loaded(written);
    QVERIFY(!loaded.isNull());
    QCOMPARE(loaded.size(), image.size());
}

QTEST_MAIN(TestClipboardToFile)
#include "test_clipboardtofile.moc"