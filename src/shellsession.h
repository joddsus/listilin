#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

// One panel's command line. It runs each command through the user's shell in
// the folder the panel shows, so pipes, globs and redirections behave as they
// do in a terminal, while the folder still follows the panel.
//
// It is not a terminal emulator: there is no pseudo-terminal, so full-screen
// programs that need one (editors, pagers) are not supported. Commands that
// only use stdin as data, such as `cat file`, work normally.
class ShellSession : public QObject
{
    Q_OBJECT

public:
    explicit ShellSession(const QString &workingDirectory, QObject *parent = nullptr);
    ~ShellSession() override;

    QString workingDirectory() const { return m_directory; }
    void setWorkingDirectory(const QString &directory);
    bool busy() const { return m_busy; }

    static QString defaultShell();
    // Splits a command the way a POSIX shell splits words, so a folder with a
    // space in it can be written as `cd "My folder"`. `complete` reports whether
    // every quote and escape was terminated.
    static QStringList splitWords(const QString &text, bool *complete = nullptr);
    // True when the command is a plain `cd` (nothing else) whose folder is a
    // literal. `target` receives the resolved folder; it is empty for `cd -`,
    // which means the previous folder. A `cd` that the shell would expand
    // (`cd $HOME`, `cd "$dir"`, `cd *`, `cd ~user`) is left to the shell, so
    // this returns false for it.
    static bool parseChangeDirectory(const QString &command, const QString &currentDirectory,
                                     QString *target);

    void run(const QString &command);
    void cancel();

signals:
    void output(const QString &text);
    void busyChanged(bool busy);
    void workingDirectoryChanged(const QString &directory);
    void finished(int exitCode);

private:
    void setBusy(bool busy);

    QProcess *m_process = nullptr;
    QString m_directory;
    QString m_previousDirectory;
    bool m_busy = false;
};
