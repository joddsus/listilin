#pragma once

#include <QString>
#include <QList>

// Named workspace layouts (Listi 0.32). Kept free of widgets so layout
// validation and (de)serialisation can be tested directly.
namespace LayoutStore {

struct Layout
{
    QString name;
    int panelCount = 1; // 1 or 2
    bool folderTree = false;
    bool places = true;
    QString sourceUri;
    QString destUri;
};

bool validName(const QString &name);
QString serialize(const Layout &layout);
bool deserialize(const QString &text, Layout *layout);

// Returns a stable, filesystem/settings-safe key for a layout name.
QString keyFor(const QString &name);

} // namespace LayoutStore