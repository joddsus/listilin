#include "clipboardtofile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>

namespace ClipboardToFile {

QString suggestedName(bool hasImage, bool hasText, const QString &textPreview)
{
    if (hasImage)
        return QStringLiteral("clipboard.png");
    if (!hasText)
        return QStringLiteral("clipboard.txt");

    // A short preview makes a readable suggested stem without risking an
    // over-long or unsafe file name.
    QString stem = textPreview.trimmed();
    if (stem.isEmpty())
        stem = QStringLiteral("clipboard");
    if (stem.size() > 24)
        stem = stem.left(24);
    // Replace characters that are not safe in file names.
    for (QChar &ch : stem) {
        if (ch == QLatin1Char('/') || ch == QLatin1Char('\\')
            || ch == QLatin1Char(':') || ch == QLatin1Char('*')
            || ch == QLatin1Char('?') || ch == QLatin1Char('"')
            || ch == QLatin1Char('<') || ch == QLatin1Char('>')
            || ch == QLatin1Char('|'))
            ch = QLatin1Char('_');
    }
    return stem + QStringLiteral(".txt");
}

QString uniquePath(const QString &directory, const QString &baseName)
{
    QString candidate = QDir(directory).filePath(baseName);
    if (!QFileInfo::exists(candidate))
        return candidate;

    const QFileInfo info(baseName);
    const QString base = info.completeBaseName().isEmpty() ? baseName : info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix();

    for (int i = 2; i < 10000; ++i) {
        candidate = QDir(directory).filePath(QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(suffix));
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    return QDir(directory).filePath(baseName);
}

bool saveText(const QString &directory, const QString &text, QString *writtenPath,
              QString *error)
{
    if (!QDir().mkpath(directory)) {
        if (error)
            *error = QObject::tr("Could not create %1").arg(directory);
        return false;
    }

    const QString name = suggestedName(false, true, text);
    const QString path = uniquePath(directory, name);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error)
            *error = QObject::tr("Could not write %1").arg(path);
        return false;
    }
    const QByteArray data = text.toUtf8();
    if (file.write(data) != data.size()) {
        if (error)
            *error = QObject::tr("Could not write %1").arg(path);
        file.close();
        QFile::remove(path);
        return false;
    }
    file.close();

    if (writtenPath)
        *writtenPath = path;
    return true;
}

bool saveImage(const QString &directory, const QImage &image, QString *writtenPath,
               QString *error)
{
    if (image.isNull()) {
        if (error)
            *error = QObject::tr("The clipboard has no image.");
        return false;
    }
    if (!QDir().mkpath(directory)) {
        if (error)
            *error = QObject::tr("Could not create %1").arg(directory);
        return false;
    }

    const QString path = uniquePath(directory, QStringLiteral("clipboard.png"));
    QImageWriter writer(path, "png");
    if (!writer.write(image)) {
        if (error)
            *error = writer.errorString();
        QFile::remove(path);
        return false;
    }

    if (writtenPath)
        *writtenPath = path;
    return true;
}

} // namespace ClipboardToFile