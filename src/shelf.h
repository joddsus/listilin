#pragma once

#include <QString>
#include <QList>

// Collects entries (local or remote URIs) into a session Shelf. Kept free of
// widgets so collection rules can be tested directly.
namespace Shelf {

struct Entry
{
    QString uri;
    QString name;
    bool isDir = false;
};

class Store
{
public:
    void add(const Entry &entry);
    void addUnique(const Entry &entry);
    void removeAt(int index);
    void clear();

    int count() const;
    Entry at(int index) const;
    QList<Entry> entries() const;

    bool contains(const QString &uri) const;
    int indexOf(const QString &uri) const;

private:
    QList<Entry> m_entries;
};

// Serialise a list of entries so a Shelf can be saved and restored by name.
// Each line is "uri\tname\t1/0"; malformed lines are ignored on decode.
QStringList encodeEntries(const QList<Entry> &entries);
QList<Entry> decodeEntries(const QStringList &lines);

} // namespace Shelf