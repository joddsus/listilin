#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "transferplan.h"

class TestTransferPlan : public QObject
{
    Q_OBJECT
private slots:
    void samePathForIdenticalText();
    void samePathThroughSymlinkedFolder();
    void movingFileOntoItselfIsAllowed();
    void folderIntoItselfRejected();
    void folderIntoOwnSubfolderRejected();
    void twoSourcesWithOneNameRejected();
    void ordinaryJobHasNoProblems();
};

void TestTransferPlan::samePathForIdenticalText()
{
    QVERIFY(TransferPlan::samePath(QStringLiteral("/tmp/a"), QStringLiteral("/tmp/./a")));
    QVERIFY(!TransferPlan::samePath(QStringLiteral("/tmp/a"), QStringLiteral("/tmp/b")));
    QVERIFY(!TransferPlan::samePath(QString(), QStringLiteral("/tmp/b")));
}

void TestTransferPlan::samePathThroughSymlinkedFolder()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString real = tmp.path() + QStringLiteral("/real");
    QVERIFY(QDir().mkpath(real));
    const QString alias = tmp.path() + QStringLiteral("/alias");
    QVERIFY(QFile::link(real, alias));

    // Two routes to one file through a symlinked parent are one file.
    QVERIFY(TransferPlan::samePath(alias + QStringLiteral("/f.txt"),
                                   real + QStringLiteral("/f.txt")));
}

void TestTransferPlan::movingFileOntoItselfIsAllowed()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString file = tmp.path() + QStringLiteral("/note.txt");
    QFile f(file);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("keep");
    f.close();

    // Move here in the folder the file already lives in is not a problem; it
    // is simply a no-op, and the file must not be copied and deleted.
    QVERIFY(TransferPlan::validate({file}, tmp.path(), true).isEmpty());
}

void TestTransferPlan::folderIntoItselfRejected()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString folder = tmp.path() + QStringLiteral("/folder");
    QVERIFY(QDir().mkpath(folder));

    const QStringList problems = TransferPlan::validate({folder}, folder, true);
    QVERIFY(!problems.isEmpty());
}

void TestTransferPlan::folderIntoOwnSubfolderRejected()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString folder = tmp.path() + QStringLiteral("/folder");
    const QString child = folder + QStringLiteral("/child");
    QVERIFY(QDir().mkpath(child));

    QVERIFY(!TransferPlan::validate({folder}, child, false).isEmpty());
    QVERIFY(!TransferPlan::validate({folder}, child, true).isEmpty());
}

void TestTransferPlan::twoSourcesWithOneNameRejected()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString first = tmp.path() + QStringLiteral("/one");
    const QString second = tmp.path() + QStringLiteral("/two");
    QVERIFY(QDir().mkpath(first));
    QVERIFY(QDir().mkpath(second));
    const QString a = first + QStringLiteral("/same.txt");
    const QString b = second + QStringLiteral("/same.txt");
    for (const QString &path : {a, b}) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
        f.close();
    }

    const QStringList problems = TransferPlan::validate({a, b}, tmp.path(), false);
    QVERIFY(!problems.isEmpty());
}

void TestTransferPlan::ordinaryJobHasNoProblems()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/source.txt");
    QFile f(source);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("data");
    f.close();

    const QString destination = tmp.path() + QStringLiteral("/dest");
    QVERIFY(QDir().mkpath(destination));
    QVERIFY(TransferPlan::validate({source}, destination, true).isEmpty());
}

QTEST_MAIN(TestTransferPlan)
#include "test_transferplan.moc"
