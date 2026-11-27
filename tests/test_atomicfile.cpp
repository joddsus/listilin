#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "atomicfile.h"

namespace {

void write(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
    f.close();
}

QByteArray read(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

} // namespace

class TestAtomicFile : public QObject
{
    Q_OBJECT
private slots:
    void publishReplacesExisting();
    void publishKeepsTargetWhenStagedIsMissing();
    void publishRefusesDirectoryTarget();
    void writeReplacesAndKeepsPermissions();
    void writeFailureKeepsOriginal();
};

void TestAtomicFile::publishReplacesExisting()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString target = tmp.path() + QStringLiteral("/target.txt");
    write(target, "old");
    const QString staged = AtomicFile::stagingPath(target, QStringLiteral("test"));
    write(staged, "new");

    QString error;
    QVERIFY(AtomicFile::publish(staged, target, &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(read(target), QByteArray("new"));
    QVERIFY(!QFileInfo::exists(staged));
}

void TestAtomicFile::publishKeepsTargetWhenStagedIsMissing()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString target = tmp.path() + QStringLiteral("/target.txt");
    write(target, "old");

    QString error;
    QVERIFY(!AtomicFile::publish(tmp.path() + QStringLiteral("/missing.tmp"), target, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(read(target), QByteArray("old"));
}

void TestAtomicFile::publishRefusesDirectoryTarget()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString target = tmp.path() + QStringLiteral("/folder");
    QVERIFY(QDir().mkpath(target));
    const QString staged = tmp.path() + QStringLiteral("/staged");
    write(staged, "new");

    QString error;
    QVERIFY(!AtomicFile::publish(staged, target, &error));
    QVERIFY(QDir(target).exists());
}

void TestAtomicFile::writeReplacesAndKeepsPermissions()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString target = tmp.path() + QStringLiteral("/notes.txt");
    write(target, "old");
    QVERIFY(QFile::setPermissions(target, QFileDevice::ReadOwner | QFileDevice::WriteOwner));

    QString error;
    QVERIFY(AtomicFile::write(target, QByteArray("new"), &error));
    QVERIFY(error.isEmpty());
    QCOMPARE(read(target), QByteArray("new"));
    const QFileDevice::Permissions permissions = QFileInfo(target).permissions();
    QVERIFY((permissions & QFileDevice::ReadOwner) != 0);
    QVERIFY((permissions & QFileDevice::WriteOwner) != 0);
    // The replaced file must not be readable or writable by anyone else.
    QCOMPARE(permissions
                 & (QFileDevice::ReadGroup | QFileDevice::WriteGroup
                    | QFileDevice::ReadOther | QFileDevice::WriteOther),
             QFileDevice::Permissions());
}

void TestAtomicFile::writeFailureKeepsOriginal()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString target = tmp.path() + QStringLiteral("/blocked");
    QVERIFY(QDir().mkpath(target));

    QString error;
    QVERIFY(!AtomicFile::write(target, QByteArray("data"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(QDir(target).exists());
}

QTEST_MAIN(TestAtomicFile)
#include "test_atomicfile.moc"
