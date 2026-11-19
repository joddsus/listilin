#include "millercolumns.h"

namespace MillerColumns {

void Navigator::setRoot(const QString &path)
{
    m_columns.clear();
    m_columns.append(path);
}

int Navigator::columnCount() const
{
    return m_columns.size();
}

QString Navigator::pathAt(int column) const
{
    if (column < 0 || column >= m_columns.size())
        return QString();
    return m_columns.at(column);
}

QString Navigator::currentPath() const
{
    if (m_columns.isEmpty())
        return QString();
    return m_columns.last();
}

void Navigator::select(int column, const QString &childPath, bool isDir)
{
    if (column < 0 || column >= m_columns.size())
        return;
    // Drop columns to the right of the selected one.
    while (m_columns.size() > column + 1)
        m_columns.removeLast();
    if (isDir && !childPath.isEmpty())
        m_columns.append(childPath);
}

void Navigator::goUp()
{
    if (m_columns.size() <= 1)
        return;
    m_columns.removeLast();
}

void Navigator::reset()
{
    if (m_columns.isEmpty())
        return;
    const QString root = m_columns.first();
    m_columns.clear();
    m_columns.append(root);
}

} // namespace MillerColumns