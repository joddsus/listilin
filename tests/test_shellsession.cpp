#include <QtTest/QtTest>

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "shellsession.h"

class TestShellSession : public QObject
{
    Q_OBJECT
private slots:
    void splitsQuotedWords();
    void reportsUnbalancedQuote();
    void recognisesPlainCd();
    void leavesCompoundCommandsToShell();
    void leavesExpandedFoldersToShell();
    void expandingChangeDirectoryReachesShell();
    void resolvesQuotedRelativeFolder();
    void expandsHome();
    void changeDirectoryUpdatesWorkingFolder();
    void returnsToPreviousFolder();
    void runsCommandInWorkingFolder();
};

void TestShellSession::splitsQuotedWords()
{
    bool complete = false;
    const QStringList words = ShellSession::splitWords(
        QStringLiteral("cp \"my file\" 'other file' plain\\ name"), &complete);
    QVERIFY(complete);
    QCOMPARE(words, QStringList({QStringLiteral("cp"), QStringLiteral("my file"),
                                 QStringLiteral("other file"), QStringLiteral("plain name")}));

    // A backslash is literal inside single quotes but escapes inside double
    // quotes, as in a POSIX shell.
    QCOMPARE(ShellSession::splitWords(QStringLiteral("cd 'a\\b'")),
             QStringList({QStringLiteral("cd"), QStringLiteral("a\\b")}));
    QCOMPARE(ShellSession::splitWords(QStringLiteral("cd \"a\\\"b\"")),
             QStringList({QStringLiteral("cd"), QStringLiteral("a\"b")}));
}

void TestShellSession::reportsUnbalancedQuote()
{
    bool complete = true;
    ShellSession::splitWords(QStringLiteral("cd \"unfinished"), &complete);
    QVERIFY(!complete);
}

void TestShellSession::recognisesPlainCd()
{
    QString target;
    QVERIFY(ShellSession::parseChangeDirectory(QStringLiteral("cd /tmp"), QStringLiteral("/"),
                                               &target));
    QCOMPARE(target, QStringLiteral("/tmp"));

    QVERIFY(ShellSession::parseChangeDirectory(QStringLiteral("cd -- /tmp"), QStringLiteral("/"),
                                               &target));
    QCOMPARE(target, QStringLiteral("/tmp"));

    QVERIFY(ShellSession::parseChangeDirectory(QStringLiteral("cd"), QStringLiteral("/"),
                                               &target));
    QCOMPARE(target, QDir::homePath());

    // An empty target means the previous folder, which the session tracks.
    QVERIFY(ShellSession::parseChangeDirectory(QStringLiteral("cd -"), QStringLiteral("/"),
                                               &target));
    QVERIFY(target.isEmpty());
}

void TestShellSession::leavesCompoundCommandsToShell()
{
    QString target;
    QVERIFY(!ShellSession::parseChangeDirectory(QStringLiteral("cd /tmp && ls"),
                                                QStringLiteral("/"), &target));
    QVERIFY(!ShellSession::parseChangeDirectory(QStringLiteral("cd /tmp; ls"),
                                                QStringLiteral("/"), &target));
    QVERIFY(!ShellSession::parseChangeDirectory(QStringLiteral("cdrom"), QStringLiteral("/"),
                                                &target));
    QVERIFY(!ShellSession::parseChangeDirectory(QStringLiteral("ls /tmp"), QStringLiteral("/"),
                                                &target));
}

void TestShellSession::leavesExpandedFoldersToShell()
{
    // The folder only exists once the shell has expanded it, so these must not be
    // resolved (or rejected) here.
    QString target;
    const QStringList expanding = {
        QStringLiteral("cd $HOME"),
        QStringLiteral("cd \"$dir\""),
        QStringLiteral("cd ${dir:-/tmp}"),
        QStringLiteral("cd $(mktemp -d)"),
        QStringLiteral("cd `dirname /a/b`"),
        QStringLiteral("cd */sub"),
        QStringLiteral("cd name?"),
        QStringLiteral("cd [ab]bc"),
        QStringLiteral("cd ~someone"),
        QStringLiteral("cd -- $dir"),
    };
    for (const QString &command : expanding) {
        target.clear();
        QVERIFY2(!ShellSession::parseChangeDirectory(command, QStringLiteral("/home/me"), &target),
                 qPrintable(command));
        QVERIFY2(target.isEmpty(), qPrintable(command));
    }

    // Quoting that removes the expansion keeps the folder literal and tracked.
    QVERIFY(ShellSession::parseChangeDirectory(QStringLiteral("cd '$HOME'"),
                                               QStringLiteral("/home/me"), &target));
    QCOMPARE(target, QStringLiteral("/home/me/$HOME"));
    QVERIFY(ShellSession::parseChangeDirectory(QStringLiteral("cd \"a*b\""),
                                               QStringLiteral("/home/me"), &target));
    QCOMPARE(target, QStringLiteral("/home/me/a*b"));
    QVERIFY(ShellSession::parseChangeDirectory(QStringLiteral("cd \\$dir"),
                                               QStringLiteral("/home/me"), &target));
    QCOMPARE(target, QStringLiteral("/home/me/$dir"));
}

void TestShellSession::expandingChangeDirectoryReachesShell()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("target")));

    ShellSession session(dir.path());
    QSignalSpy output(&session, &ShellSession::output);
    QSignalSpy finished(&session, &ShellSession::finished);

    const QString target = QDir(dir.path()).filePath(QStringLiteral("target"));
    qputenv("LISTISTA_TEST_FOLDER", target.toLocal8Bit());
    session.run(QStringLiteral("cd $LISTISTA_TEST_FOLDER"));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);

    QString text;
    for (const QList<QVariant> &arguments : output)
        text.append(arguments.first().toString());
    QVERIFY2(!text.contains(QStringLiteral("not a folder")), qPrintable(text));
    QVERIFY(!session.busy());
    // The shell changed folder in its own process, so ours is unchanged: the
    // command was not intercepted here.
    QCOMPARE(session.workingDirectory(), QDir::cleanPath(dir.path()));
}

void TestShellSession::resolvesQuotedRelativeFolder()
{
    QString target;
    QVERIFY(ShellSession::parseChangeDirectory(QStringLiteral("cd \"my folder\""),
                                               QStringLiteral("/home/me"), &target));
    QCOMPARE(target, QStringLiteral("/home/me/my folder"));
    QVERIFY(ShellSession::parseChangeDirectory(QStringLiteral("cd ../up"), QStringLiteral("/a/b"),
                                               &target));
    QCOMPARE(target, QStringLiteral("/a/up"));
}

void TestShellSession::expandsHome()
{
    QString target;
    QVERIFY(ShellSession::parseChangeDirectory(QStringLiteral("cd ~/Notes"), QStringLiteral("/"),
                                               &target));
    QCOMPARE(target, QDir::homePath() + QStringLiteral("/Notes"));
}

void TestShellSession::changeDirectoryUpdatesWorkingFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("sub folder")));

    ShellSession session(dir.path());
    QSignalSpy changed(&session, &ShellSession::workingDirectoryChanged);
    session.run(QStringLiteral("cd \"sub folder\""));

    QCOMPARE(session.workingDirectory(),
             QDir::cleanPath(dir.path() + QStringLiteral("/sub folder")));
    QCOMPARE(changed.count(), 1);
    QVERIFY(!session.busy());
}

void TestShellSession::returnsToPreviousFolder()
{
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());

    ShellSession session(first.path());
    session.setWorkingDirectory(second.path());
    session.run(QStringLiteral("cd -"));

    QCOMPARE(session.workingDirectory(), QDir::cleanPath(first.path()));
}

void TestShellSession::runsCommandInWorkingFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile file(QDir(dir.path()).filePath(QStringLiteral("note.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hello from the folder\n");
    file.close();

    ShellSession session(dir.path());
    QSignalSpy output(&session, &ShellSession::output);
    QSignalSpy finished(&session, &ShellSession::finished);

    session.run(QStringLiteral("cat note.txt"));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);

    QString text;
    for (const QList<QVariant> &arguments : output)
        text.append(arguments.first().toString());
    QVERIFY2(text.contains(QStringLiteral("hello from the folder")), qPrintable(text));
    QVERIFY(!session.busy());
}

QTEST_MAIN(TestShellSession)
#include "test_shellsession.moc"
