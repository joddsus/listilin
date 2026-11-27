#include "shellsession.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

ShellSession::ShellSession(const QString &workingDirectory, QObject *parent)
    : QObject(parent)
    , m_directory(QDir::cleanPath(workingDirectory.isEmpty() ? QDir::homePath()
                                                             : workingDirectory))
{
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        emit output(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus status) {
                if (!m_busy)
                    return;
                setBusy(false);
                if (status == QProcess::CrashExit)
                    emit output(tr("\n[command stopped]\n"));
                emit finished(exitCode);
            });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        emit output(tr("\nCould not start %1: %2\n").arg(defaultShell(), m_process->errorString()));
        setBusy(false);
    });
}

ShellSession::~ShellSession()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

QString ShellSession::defaultShell()
{
    const QString shell = qEnvironmentVariable("SHELL").trimmed();
    return shell.isEmpty() ? QStringLiteral("sh") : shell;
}

QStringList ShellSession::splitWords(const QString &text, bool *complete)
{
    QStringList words;
    QString current;
    bool inWord = false;
    bool escaped = false;
    QChar quote;

    for (const QChar character : text) {
        // Inside single quotes every character is literal, including a
        // backslash; double quotes still honour a backslash escape.
        if (quote == QLatin1Char('\'')) {
            if (character == quote)
                quote = QChar();
            else
                current.append(character);
            continue;
        }
        if (escaped) {
            current.append(character);
            inWord = true;
            escaped = false;
            continue;
        }
        if (character == QLatin1Char('\\')) {
            escaped = true;
            inWord = true;
            continue;
        }
        if (!quote.isNull()) {
            if (character == quote)
                quote = QChar();
            else
                current.append(character);
            continue;
        }
        if (character == QLatin1Char('\'') || character == QLatin1Char('"')) {
            quote = character;
            inWord = true;
            continue;
        }
        if (character.isSpace()) {
            if (inWord) {
                words.append(current);
                current.clear();
                inWord = false;
            }
            continue;
        }
        current.append(character);
        inWord = true;
    }

    if (escaped)
        current.append(QLatin1Char('\\'));
    if (inWord)
        words.append(current);

    if (complete)
        *complete = quote.isNull();
    return words;
}

// True when a POSIX shell would expand something in the command before `cd`
// sees the folder: variables, command substitution, globs or a `~user` home.
// Those commands have to go to the shell itself, because only it knows the
// result, and we must not reject them as a missing folder.
static bool usesShellExpansion(const QString &command)
{
    QChar quote;
    bool escaped = false;

    for (const QChar character : command) {
        if (quote == QLatin1Char('\'')) {
            if (character == quote)
                quote = QChar();
            continue;
        }
        if (escaped) {
            escaped = false;
            continue;
        }
        if (character == QLatin1Char('\\')) {
            escaped = true;
            continue;
        }
        if (!quote.isNull()) {
            // Inside double quotes only variables and command substitution
            // expand; a `*` or `?` there is a plain character.
            if (character == quote)
                quote = QChar();
            else if (character == QLatin1Char('$') || character == QLatin1Char('`'))
                return true;
            continue;
        }
        if (character == QLatin1Char('\'') || character == QLatin1Char('"')) {
            quote = character;
            continue;
        }
        switch (character.unicode()) {
        case '$':
        case '`':
        case '*':
        case '?':
        case '[':
        case '{':
            return true;
        default:
            break;
        }
    }
    return false;
}

bool ShellSession::parseChangeDirectory(const QString &command, const QString &currentDirectory,
                                        QString *target)
{
    if (usesShellExpansion(command))
        return false;

    bool complete = false;
    const QStringList words = splitWords(command.trimmed(), &complete);
    if (!complete || words.isEmpty() || words.first() != QLatin1String("cd"))
        return false;

    // Anything longer than `cd [--] folder` is a compound command (`cd /tmp &&
    // make`), which only the shell can run.
    QString argument;
    if (words.size() == 2) {
        argument = words.at(1);
    } else if (words.size() == 3 && words.at(1) == QLatin1String("--")) {
        argument = words.at(2);
    } else if (words.size() != 1) {
        return false;
    }

    if (argument == QLatin1String("-")) {
        target->clear();
        return true;
    }

    if (argument.isEmpty() || argument == QLatin1String("~"))
        argument = QDir::homePath();
    else if (argument.startsWith(QLatin1String("~/")))
        argument = QDir::homePath() + argument.mid(1);
    else if (argument.startsWith(QLatin1Char('~')))
        return false; // `~user`, which only the shell can resolve

    const QString resolved = QDir::isAbsolutePath(argument)
        ? argument
        : QDir(currentDirectory).absoluteFilePath(argument);
    *target = QDir::cleanPath(resolved);
    return true;
}

void ShellSession::setWorkingDirectory(const QString &directory)
{
    if (directory.trimmed().isEmpty())
        return;

    const QString cleaned = QDir::cleanPath(directory);
    if (cleaned == m_directory)
        return;

    m_previousDirectory = m_directory;
    m_directory = cleaned;
    emit workingDirectoryChanged(m_directory);
}

void ShellSession::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged(m_busy);
}

void ShellSession::run(const QString &command)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty())
        return;

    if (m_busy) {
        emit output(tr("[a command is still running — stop it first]\n"));
        return;
    }

    QString target;
    if (parseChangeDirectory(trimmed, m_directory, &target)) {
        if (target.isEmpty()) {
            if (m_previousDirectory.isEmpty()) {
                emit output(tr("cd: no previous folder\n"));
                return;
            }
            target = m_previousDirectory;
        }
        if (!QFileInfo(target).isDir()) {
            emit output(tr("cd: %1: not a folder\n").arg(target));
            return;
        }
        setWorkingDirectory(target);
        return;
    }

    m_process->setWorkingDirectory(m_directory);
    setBusy(true);
    m_process->start(defaultShell(), {QStringLiteral("-c"), trimmed});
    if (!m_process->waitForStarted(3000)) {
        emit output(tr("\nCould not start %1: %2\n").arg(defaultShell(), m_process->errorString()));
        setBusy(false);
    }
}

void ShellSession::cancel()
{
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return;
    m_process->terminate();
    if (!m_process->waitForFinished(1500)) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
    setBusy(false);
}
