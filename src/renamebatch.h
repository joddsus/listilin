#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// Name transformation for the mass-rename dialog, kept free of widgets and
// filesystem access so the rules can be tested directly.
//
// The last dot starts the extension, except for dotfiles and names ending in a
// dot: ".bashrc" is all stem, not an invisible base name plus a "bashrc"
// extension. Whether the extension is kept is the caller's choice.
namespace RenameBatch {

struct Options
{
    QString find;
    QString replace;
    QString prefix;
    QString suffix;
    bool keepExtension = true;
    bool numbering = false;
    int startNumber = 1;
};

struct Plan
{
    QStringList currentNames;
    QStringList newNames;
    QVector<bool> duplicate; // newNames[i] repeats an earlier proposed name
    QVector<bool> unchanged; // newNames[i] equals currentNames[i]
    QVector<bool> invalid;   // newNames[i] is not a safe file name

    int renameCount() const;
};

Plan plan(const QStringList &names, const Options &options);

} // namespace RenameBatch
