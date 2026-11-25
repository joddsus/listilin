#include "undostack.h"

namespace UndoStack {

Stack::Stack(int maxSize)
    : m_maxSize(maxSize > 0 ? maxSize : 64)
{
}

void Stack::push(const Entry &entry)
{
    if (!entry.undoable)
        return;
    if (m_entries.size() >= m_maxSize)
        m_entries.removeFirst();
    m_entries.append(entry);
}

bool Stack::canUndo() const
{
    return !m_entries.isEmpty();
}

Entry Stack::pop()
{
    if (m_entries.isEmpty())
        return Entry();
    Entry entry = m_entries.takeLast();
    return entry;
}

void Stack::clear()
{
    m_entries.clear();
}

int Stack::size() const
{
    return m_entries.size();
}

int Stack::maxSize() const
{
    return m_maxSize;
}

void Stack::setMaxSize(int size)
{
    if (size <= 0)
        return;
    m_maxSize = size;
    while (m_entries.size() > m_maxSize)
        m_entries.removeFirst();
}

bool Stack::peek(Entry *entry) const
{
    if (!entry || m_entries.isEmpty())
        return false;
    *entry = m_entries.last();
    return true;
}

} // namespace UndoStack