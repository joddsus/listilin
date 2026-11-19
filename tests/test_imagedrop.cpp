#include <QtTest/QtTest>
#include <QImage>
#include <QTemporaryDir>

#include "imagedrop.h"

class TestImageDrop : public QObject
{
    Q_OBJECT
private slots:
    void extensionForFormat();
    void suggestedName();
    void convertImageInMemory();
    void convertImageFromPath();
    void isImageFile();
};

void TestImageDrop::extensionForFormat()
{
    QCOMPARE(ImageDrop::extensionForFormat(QStringLiteral("PNG")), QStringLiteral("png"));
    QCOMPARE(ImageDrop::extensionForFormat(QStringLiteral("jpeg")), QStringLiteral("jpg"));
    QVERIFY(ImageDrop::extensionForFormat(QStringLiteral("nope")).isEmpty());
}

void TestImageDrop::suggestedName()
{
    QCOMPARE(ImageDrop::suggestedName(QStringLiteral("/tmp/foo.bar"), QStringLiteral("png")),
             QStringLiteral("foo.png"));
}

void TestImageDrop::convertImageInMemory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString target = dir.path() + QStringLiteral("/out.jpg");
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::red);
    QString error;
    QVERIFY(ImageDrop::convertImage(image, target, QStringLiteral("jpg"), &error));
    QVERIFY(QFileInfo::exists(target));
}

void TestImageDrop::convertImageFromPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.path() + QStringLiteral("/src.png");
    const QString target = dir.path() + QStringLiteral("/out.bmp");
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(Qt::blue);
    QVERIFY(image.save(source, "png"));
    QString error;
    QVERIFY(ImageDrop::convertImage(source, target, QStringLiteral("bmp"), &error));
    QVERIFY(QFileInfo::exists(target));
}

void TestImageDrop::isImageFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.path() + QStringLiteral("/src.png");
    QImage image(4, 4, QImage::Format_RGB32);
    image.fill(Qt::green);
    QVERIFY(image.save(source, "png"));
    QVERIFY(ImageDrop::isImageFile(source));
    QVERIFY(!ImageDrop::isImageFile(dir.path() + QStringLiteral("/nope.xxx")));
}

QTEST_MAIN(TestImageDrop)
#include "test_imagedrop.moc"