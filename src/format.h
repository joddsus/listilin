#pragma once

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QString>

inline QString humanSize(qint64 bytes)
{
    static const char *units[] = {"B", "K", "M", "G", "T", "P"};

    if (bytes < 1024)
        return QString::number(bytes) + QStringLiteral(" B");

    double value = static_cast<double>(bytes) / 1024.0;
    int unit = 1;
    while (value >= 1024.0 && unit < 5) {
        value /= 1024.0;
        ++unit;
    }
    return QString::number(value, 'f', value < 10.0 ? 1 : 0) + QLatin1Char(' ')
        + QLatin1String(units[unit]);
}

// "1 item" / "12 items" — written out instead of %n, because "item(s)" is not
// a sentence anyone wants to read.
inline QString itemCount(int count)
{
    return count == 1 ? QObject::tr("1 item") : QObject::tr("%1 items").arg(count);
}

inline QString shortDate(const QDateTime &dt)
{
    if (!dt.isValid())
        return QStringLiteral("--");

    const QDate date = dt.date();
    if (date.year() == QDate::currentDate().year())
        return date.toString(QStringLiteral("dd.MM")) + QLatin1Char(' ')
            + dt.time().toString(QStringLiteral("HH:mm"));

    return date.toString(QStringLiteral("yyyy-MM-dd"));
}
