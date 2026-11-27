#pragma once

#include <QString>
#include <QVector>

// What an upload actually produced, in enough detail to undo it: where the
// original lived (empty when the source was remote/archive and was deliberately
// kept), where the copy was written, and the digest that proves the copy is
// still the one this program made. Shared by the upload path and the undo
// journal so both agree on what a reversible upload is.
namespace UploadRecord {

struct Item
{
    QString sourcePath;      // original local path; empty when nothing was moved
    QString destinationUri;  // the uploaded member, for reading back and removal
    QString digest;          // SHA-256 of the uploaded file; empty for a folder
    bool isDir = false;
    bool createdHere = false; // the destination folder was created by the upload
};

} // namespace UploadRecord
