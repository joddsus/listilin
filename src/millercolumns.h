#pragma once

#include <QString>
#include <QList>

// Model for Miller-column navigation: a stack of selected paths, one per
// column. Kept free of widgets so navigation rules can be tested.
namespace MillerColumns {

class Navigator
{
public:
    void setRoot(const QString &path);

    int columnCount() const;
    QString pathAt(int column) const;
    QString currentPath() const;

    // Select an entry in `column`, replacing columns to its right. `childPath`
    // is the path of the selected entry (may be a directory to open next).
    void select(int column, const QString &childPath, bool isDir);

    void goUp();
    void reset();

private:
    QList<QString> m_columns;
};

} // namespace MillerColumns