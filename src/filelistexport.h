#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

// CSV export of the visible or selected entries in a panel. Kept free of widgets
// so the exact Listi-compatible quoting and timestamp rules can be tested.
namespace FileListExport {

struct Row
{
    QString name;
    QString fullPath;
    QString type;   // "file", "folder" or "link"
    QString size;   // bytes for regular files; empty for folders/unknown
    QString modified; // UTC ISO-8601, empty when unknown
};

QString header();
QString toCsv(const QVector<Row> &rows);
// Writes a new CSV file. Existing files are never replaced.
bool writeNew(const QString &path, const QVector<Row> &rows, QString *error);

QString textCell(const QString &text);
QString modifiedUtc(const QDateTime &modified);

} // namespace FileListExport
