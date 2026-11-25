#include "terminallauncher.h"

#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QStandardPaths>

namespace TerminalLauncher {

QStringList candidatePrograms()
{
    QStringList programs;
    const QString fromEnvironment = qEnvironmentVariable("TERMINAL").trimmed();
    if (!fromEnvironment.isEmpty())
        programs.append(fromEnvironment);

    programs << QStringLiteral("x-terminal-emulator")
             << QStringLiteral("gnome-terminal")
             << QStringLiteral("konsole")
             << QStringLiteral("xfce4-terminal")
             << QStringLiteral("mate-terminal")
             << QStringLiteral("lxterminal")
             << QStringLiteral("tilix")
             << QStringLiteral("terminator")
             << QStringLiteral("kitty")
             << QStringLiteral("alacritty")
             << QStringLiteral("foot")
             << QStringLiteral("wezterm")
             << QStringLiteral("xterm")
             << QStringLiteral("urxvt")
             << QStringLiteral("rxvt")
             << QStringLiteral("open")
             << QStringLiteral("cmd.exe");
    return programs;
}

Command commandFor(const QString &program, const QString &workingDir)
{
    Command command;
    command.program = program;
    const QString base = QFileInfo(program).fileName().toLower();
    const QString shell = qEnvironmentVariable("SHELL", QStringLiteral("/bin/sh"));

    if (base == QLatin1String("gnome-terminal") || base == QLatin1String("mate-terminal")
        || base == QLatin1String("xfce4-terminal") || base == QLatin1String("lxterminal")
        || base == QLatin1String("terminator") || base == QLatin1String("tilix")
        || base == QLatin1String("foot")) {
        command.arguments << QStringLiteral("--working-directory=") + workingDir;
    } else if (base == QLatin1String("konsole")) {
        command.arguments << QStringLiteral("--workdir") << workingDir;
    } else if (base == QLatin1String("kitty")) {
        command.arguments << QStringLiteral("--directory") << workingDir;
    } else if (base == QLatin1String("alacritty")) {
        command.arguments << QStringLiteral("--working-directory") << workingDir;
    } else if (base == QLatin1String("wezterm")) {
        command.arguments << QStringLiteral("start") << QStringLiteral("--cwd") << workingDir;
    } else if (base == QLatin1String("open")) {
        command.arguments << QStringLiteral("-a") << QStringLiteral("Terminal") << workingDir;
    } else if (base == QLatin1String("cmd") || base == QLatin1String("cmd.exe")) {
        command.arguments << QStringLiteral("/K")
                          << (QStringLiteral("cd /d \"") + workingDir + QLatin1Char('"'));
    } else {
        // xterm, urxvt, rxvt and unknown terminals: run the user's shell with
        // positional arguments, so spaces and quotes in the folder are safe.
        command.arguments << QStringLiteral("-e") << QStringLiteral("sh")
                          << QStringLiteral("-c")
                          << QStringLiteral("cd -- \"$1\" && exec \"$2\"")
                          << QStringLiteral("sh") << workingDir << shell;
    }
    return command;
}

bool launch(const QString &workingDir, QString *error)
{
    for (const QString &candidate : candidatePrograms()) {
        const QString executable = QStandardPaths::findExecutable(candidate);
        if (executable.isEmpty())
            continue;
        const Command command = commandFor(executable, workingDir);
        if (QProcess::startDetached(command.program, command.arguments, workingDir))
            return true;
    }

    if (error)
        *error = QObject::tr("No terminal emulator was found. Set TERMINAL to your terminal program.");
    return false;
}

} // namespace TerminalLauncher
