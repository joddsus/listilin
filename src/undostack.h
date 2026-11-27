#pragma once

#include <QString>
#include <QStringList>

#include <QList>

// Reversible operation journal for file-manager actions. Kept free of widgets
// so the stacking and validation rules can be tested without a GUI. The
// journal records enough information to reverse a completed operation; the
// actual file operations are performed by the caller.
namespace UndoStack {

enum class OpType
{
    Rename,   // source -> destination (single item)
    Mkdir,    // created directory at destination
    Trash,    // sources moved to trash (trashNames recorded)
    Copy,     // sources copied to destinations
    Move,     // sources moved to destinations
};

struct Entry
{
    OpType type = OpType::Rename;
    QStringList sources;      // original paths
    QStringList destinations; // resulting paths
    QStringList trashNames;   // freedesktop trash entry names (Trash only)
    // Fingerprints of the destinations at the moment the job finished. Undo
    // refuses to delete or move back an entry that has changed since.
    QStringList fingerprints;
    bool undoable = true;
};

class Stack
{
public:
    explicit Stack(int maxSize = 64);

    void push(const Entry &entry);
    bool canUndo() const;
    Entry pop();
    void clear();
    int size() const;
    int maxSize() const;
    void setMaxSize(int size);

    // Returns the most recent entry without removing it.
    bool peek(Entry *entry) const;

private:
    QList<Entry> m_entries;
    int m_maxSize = 64;
};

} // namespace UndoStack