#include "imagedrop.h"

#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>

namespace ImageDrop {

bool isImageFile(const QString &path)
{
    QImageReader reader(path);
    return reader.canRead();
}

QString extensionForFormat(const QString &format)
{
    const QString f = format.trimmed().toLower();
    if (f == QLatin1String("png"))
        return QStringLiteral("png");
    if (f == QLatin1String("jpg") || f == QLatin1String("jpeg"))
        return QStringLiteral("jpg");
    if (f == QLatin1String("webp"))
        return QStringLiteral("webp");
    if (f == QLatin1String("bmp"))
        return QStringLiteral("bmp");
    if (f == QLatin1String("gif"))
        return QStringLiteral("gif");
    if (f == QLatin1String("tiff") || f == QLatin1String("tif"))
        return QStringLiteral("tiff");
    return QString();
}

QString suggestedName(const QString &sourcePath, const QString &format)
{
    const QString ext = extensionForFormat(format);
    if (ext.isEmpty())
        return QString();
    const QFileInfo info(sourcePath);
    const QString base = info.completeBaseName().isEmpty() ? QStringLiteral("image") : info.completeBaseName();
    return base + QLatin1Char('.') + ext;
}

bool convertImage(const QImage &image, const QString &targetPath,
                  const QString &format, QString *error)
{
    if (image.isNull()) {
        if (error)
            *error = QObject::tr("No image to convert.");
        return false;
    }
    const QString fmt = format.trimmed().toLower();
    if (fmt.isEmpty()) {
        if (error)
            *error = QObject::tr("Unknown image format.");
        return false;
    }
    QImageWriter writer(targetPath, fmt.toUtf8());
    if (!writer.canWrite()) {
        if (error)
            *error = QObject::tr("Cannot write %1.").arg(format);
        return false;
    }
    if (!writer.write(image)) {
        if (error)
            *error = writer.errorString();
        return false;
    }
    return true;
}

bool convertImage(const QString &sourcePath, const QString &targetPath,
                  const QString &format, QString *error)
{
    QImageReader reader(sourcePath);
    if (!reader.canRead()) {
        if (error)
            *error = QObject::tr("Cannot read %1.").arg(sourcePath);
        return false;
    }
    const QImage image = reader.read();
    if (image.isNull()) {
        if (error)
            *error = reader.errorString();
        return false;
    }
    return convertImage(image, targetPath, format, error);
}

} // namespace ImageDrop