#include "filelistexport.h"

#include <QDateTime>
#include <QFile>
#include <QObject>
#include <QFileInfo>
#include <QIODevice>

namespace FileListExport {

QString header()
{
    return QStringLiteral("Name,Full path,Type,Size (bytes),Modified (UTC)\r\n");
}

QString textCell(const QString &text)
{
    QString cell = QStringLiteral("\"");
    const QChar first = text.isEmpty() ? QChar() : text.at(0);
    int firstNonSpace = -1;
    for (int i = 0; i < text.size(); ++i) {
        if (!text.at(i).isSpace()) {
            firstNonSpace = i;
            break;
        }
    }
    // Spreadsheet programs treat these leading characters as formulas. Listi
    // prefixes the cell text with an apostrophe before quoting it.
    const bool startsRisky = first == QLatin1Char('\t') || first == QLatin1Char('\r')
        || first == QLatin1Char('\n');
    const bool formula = firstNonSpace >= 0
        && QStringLiteral("=+-@").contains(text.at(firstNonSpace));
    if (startsRisky || formula)
        cell += QLatin1Char('\'');
    for (const QChar c : text) {
        if (c == QLatin1Char('"'))
            cell += QLatin1Char('"');
        cell += c;
    }
    cell += QLatin1Char('"');
    return cell;
}

QString modifiedUtc(const QDateTime &modified)
{
    if (!modified.isValid())
        return QString();
    return modified.toUTC().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ssZ"));
}

QString toCsv(const QVector<Row> &rows)
{
    QString output = header();
    for (const Row &row : rows) {
        output += textCell(row.name) + QLatin1Char(',')
            + textCell(row.fullPath) + QLatin1Char(',')
            + row.type + QLatin1Char(',')
            + row.size + QLatin1Char(',')
            + row.modified + QStringLiteral("\r\n");
    }
    return output;
}

bool writeNew(const QString &path, const QVector<Row> &rows, QString *error)
{
    if (QFileInfo::exists(path)) {
        if (error)
            *error = QObject::tr("The file already exists. Choose a new filename.");
        return false;
    }

    QFile file(path);
    // NewOnly closes the race between the existence check and creation, and
    // guarantees that an export can never replace an existing file.
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        if (error)
            *error = QObject::tr("Could not create the CSV file.");
        return false;
    }

    const QByteArray data = toCsv(rows).toUtf8();
    bool ok = file.write(data) == data.size();
    ok = file.flush() && ok;
    file.close();
    if (!ok) {
        QFile::remove(path);
        if (error)
            *error = QObject::tr("Could not finish writing the CSV file.");
        return false;
    }
    return true;
}

} // namespace FileListExport
