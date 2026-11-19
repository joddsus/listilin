#pragma once

#include <QString>
#include <QImage>

// Helpers for dropping or pasting images and converting them to a chosen
// format. Kept free of widgets so the conversion rules can be tested.
namespace ImageDrop {

bool isImageFile(const QString &path);

// Returns the file extension (without dot) that `format` implies, or empty
// when the format is unknown.
QString extensionForFormat(const QString &format);

// Suggested destination file name for converting `sourcePath` to `format`.
QString suggestedName(const QString &sourcePath, const QString &format);

// Converts the image at `sourcePath` to `targetPath` in `format` (e.g. "png",
// "jpg", "webp"). Returns true on success.
bool convertImage(const QString &sourcePath, const QString &targetPath,
                  const QString &format, QString *error);

// Converts an in-memory image to `targetPath` in `format`.
bool convertImage(const QImage &image, const QString &targetPath,
                  const QString &format, QString *error);

} // namespace ImageDrop