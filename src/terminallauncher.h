#pragma once

#include <QString>
#include <QStringList>

// Starts a terminal in a folder using whichever common terminal is installed.
// Kept separate from the window so the argument rules can be tested without
// launching a GUI.
namespace TerminalLauncher {

struct Command
{
    QString program;
    QStringList arguments;
};

// TERMINAL is tried first, then the usual Linux/BSD terminals. Returns an empty
// list when none is installed.
QStringList candidatePrograms();
Command commandFor(const QString &program, const QString &workingDir);
bool launch(const QString &workingDir, QString *error = nullptr);

} // namespace TerminalLauncher
