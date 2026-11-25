#include <QtTest/QtTest>

#include "terminallauncher.h"

class TestTerminalLauncher : public QObject
{
    Q_OBJECT
private slots:
    void gnomeTerminalUsesWorkingDirectory();
    void konsoleUsesWorkdir();
    void xtermUsesSafeShellCommand();
    void unknownTerminalFallsBackToShell();
};

void TestTerminalLauncher::gnomeTerminalUsesWorkingDirectory()
{
    const TerminalLauncher::Command command
        = TerminalLauncher::commandFor(QStringLiteral("gnome-terminal"), QStringLiteral("/tmp/a b"));
    QCOMPARE(command.program, QStringLiteral("gnome-terminal"));
    QCOMPARE(command.arguments, QStringList({QStringLiteral("--working-directory=/tmp/a b")}));
}

void TestTerminalLauncher::konsoleUsesWorkdir()
{
    const TerminalLauncher::Command command
        = TerminalLauncher::commandFor(QStringLiteral("konsole"), QStringLiteral("/tmp/x"));
    QCOMPARE(command.arguments, QStringList({QStringLiteral("--workdir"), QStringLiteral("/tmp/x")}));
}

void TestTerminalLauncher::xtermUsesSafeShellCommand()
{
    const TerminalLauncher::Command command
        = TerminalLauncher::commandFor(QStringLiteral("xterm"), QStringLiteral("/tmp/x"));
    QCOMPARE(command.arguments.first(), QStringLiteral("-e"));
    QVERIFY(command.arguments.contains(QStringLiteral("cd -- \"$1\" && exec \"$2\"")));
    QCOMPARE(command.arguments.at(command.arguments.size() - 2), QStringLiteral("/tmp/x"));
}

void TestTerminalLauncher::unknownTerminalFallsBackToShell()
{
    const TerminalLauncher::Command command
        = TerminalLauncher::commandFor(QStringLiteral("my-term"), QStringLiteral("/tmp/x"));
    QVERIFY(command.arguments.contains(QStringLiteral("sh")));
    QVERIFY(command.arguments.contains(QStringLiteral("/tmp/x")));
}

QTEST_MAIN(TestTerminalLauncher)
#include "test_terminallauncher.moc"
