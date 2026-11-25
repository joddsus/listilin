#include "shelf.h"

namespace Shelf {

void Store::add(const Entry &entry)
{
    m_entries.append(entry);
}

void Store::addUnique(const Entry &entry)
{
    if (contains(entry.uri))
        return;
    m_entries.append(entry);
}

void Store::removeAt(int index)
{
    if (index < 0 || index >= m_entries.size())
        return;
    m_entries.removeAt(index);
}

void Store::clear()
{
    m_entries.clear();
}

int Store::count() const
{
    return m_entries.size();
}

Entry Store::at(int index) const
{
    if (index < 0 || index >= m_entries.size())
        return Entry();
    return m_entries.at(index);
}

QList<Entry> Store::entries() const
{
    return m_entries;
}

bool Store::contains(const QString &uri) const
{
    return indexOf(uri) >= 0;
}

int Store::indexOf(const QString &uri) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).uri == uri)
            return i;
    }
    return -1;
}

QStringList encodeEntries(const QList<Entry> &entries)
{
    QStringList lines;
    for (const Entry &entry : entries) {
        lines.append(entry.uri + QLatin1Char('\t') + entry.name + QLatin1Char('\t') +
                     (entry.isDir ? QStringLiteral("1") : QStringLiteral("0")));
    }
    return lines;
}

QList<Entry> decodeEntries(const QStringList &lines)
{
    QList<Entry> entries;
    for (const QString &line : lines) {
        const QStringList parts = line.split(QLatin1Char('\t'));
        if (parts.size() < 2)
            continue;
        Entry entry;
        entry.uri = parts.at(0);
        entry.name = parts.at(1);
        entry.isDir = (parts.size() > 2 && parts.at(2) == QStringLiteral("1"));
        if (entry.uri.isEmpty())
            continue;
        entries.append(entry);
    }
    return entries;
}

} // namespace Shelf