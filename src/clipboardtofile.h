#pragma once

#include <QImage>
#include <QString>

// Saving clipboard text or images to a file in the active folder. Kept free of
// widgets so the naming and writing rules can be tested directly.
namespace ClipboardToFile {

// Suggested file name for a clipboard payload.
QString suggestedName(bool hasImage, bool hasText, const QString &textPreview);

// Returns a path in `directory` that does not yet exist, based on `baseName`.
QString uniquePath(const QString &directory, const QString &baseName);

// Writes `text` as UTF-8 to a non-clobbering file in `directory`.
// Returns the written path in `writtenPath` on success.
bool saveText(const QString &directory, const QString &text, QString *writtenPath,
              QString *error);

// Writes `image` as PNG to a non-clobbering file in `directory`.
bool saveImage(const QString &directory, const QImage &image, QString *writtenPath,
               QString *error);

} // namespace ClipboardToFile