#include <QtTest/QtTest>

#include <QDir>
#include <QFile>

#include "propertiesbatch.h"

class TestPropertiesBatch : public QObject
{
    Q_OBJECT
private slots:
    void summarizesFilesAndFolders();
    void reportsMultipleFolders();
    void appliesToSelectionOnly();
    void appliesRecursively();
    void preservesPartiallyChangedBits();
};

namespace {

QString writeFile(const QString &dir, const QString &name, const QByteArray &contents)
{
    const QString path = QDir(dir).filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(contents);
    file.close();
    return path;
}

void setPermissions(const QString &path, QFile::Permissions permissions)
{
    QFile file(path);
    file.setPermissions(permissions);
}

} // namespace

void TestPropertiesBatch::summarizesFilesAndFolders()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString top = writeFile(dir.path(), QStringLiteral("a.txt"), QByteArray("abc"));
    QVERIFY(!top.isEmpty());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("sub")));
    const QString inner = writeFile(dir.path() + QStringLiteral("/sub"),
                                    QStringLiteral("b.txt"), QByteArray("12345"));
    QVERIFY(!inner.isEmpty());

    const PropertiesBatch::Summary summary =
        PropertiesBatch::summarize({top, dir.path() + QStringLiteral("/sub")});
    QCOMPARE(summary.files, 1);
    QCOMPARE(summary.folders, 1);
    QCOMPARE(summary.totalSize, qint64(3));
    QCOMPARE(summary.location, QDir(dir.path()).absolutePath());
}

void TestPropertiesBatch::reportsMultipleFolders()
{
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    const QString a = writeFile(first.path(), QStringLiteral("a"), QByteArray("a"));
    const QString b = writeFile(second.path(), QStringLiteral("b"), QByteArray("b"));
    QVERIFY(!a.isEmpty());
    QVERIFY(!b.isEmpty());

    const PropertiesBatch::Summary summary = PropertiesBatch::summarize({a, b});
    QCOMPARE(summary.files, 2);
    QCOMPARE(summary.folders, 0);
    QCOMPARE(summary.totalSize, qint64(2));
    QVERIFY(summary.location.isEmpty());
}

void TestPropertiesBatch::appliesToSelectionOnly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString top = writeFile(dir.path(), QStringLiteral("top"), QByteArray("top"));
    QVERIFY(!top.isEmpty());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("sub")));
    const QString inner = writeFile(dir.path() + QStringLiteral("/sub"),
                                    QStringLiteral("inner"), QByteArray("inner"));
    QVERIFY(!inner.isEmpty());
    setPermissions(top, QFile::ReadOwner | QFile::WriteOwner);
    setPermissions(inner, QFile::ReadOwner | QFile::WriteOwner);

    PropertiesBatch::Change change;
    change.set = QFile::ReadOwner;
    change.clear = QFile::WriteOwner;
    const QStringList failures = PropertiesBatch::apply({top}, change, false);
    QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));

    QVERIFY(QFileInfo(top).permissions() & QFile::ReadOwner);
    QVERIFY(!(QFileInfo(top).permissions() & QFile::WriteOwner));
    // The nested file was not selected, so it keeps its write bit.
    QVERIFY(QFileInfo(inner).permissions() & QFile::WriteOwner);
}

void TestPropertiesBatch::appliesRecursively()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("sub/deeper")));
    const QString inner = writeFile(dir.path() + QStringLiteral("/sub/deeper"),
                                    QStringLiteral("inner"), QByteArray("inner"));
    QVERIFY(!inner.isEmpty());
    const QString top = writeFile(dir.path(), QStringLiteral("top"), QByteArray("top"));
    QVERIFY(!top.isEmpty());
    setPermissions(inner, QFile::ReadOwner | QFile::WriteOwner);
    setPermissions(top, QFile::ReadOwner | QFile::WriteOwner);

    PropertiesBatch::Change change;
    change.set = QFile::ReadOwner;
    change.clear = QFile::WriteOwner;
    const QStringList failures = PropertiesBatch::apply({dir.path()}, change, true);
    QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));

    QVERIFY(!(QFileInfo(inner).permissions() & QFile::WriteOwner));
    QVERIFY(!(QFileInfo(top).permissions() & QFile::WriteOwner));
    QVERIFY(QFileInfo(inner).permissions() & QFile::ReadOwner);

    // Put the tree back so QTemporaryDir can remove read-only entries.
    const QFile::Permissions writable =
        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
    setPermissions(dir.path(), writable);
    setPermissions(dir.path() + QStringLiteral("/sub"), writable);
    setPermissions(dir.path() + QStringLiteral("/sub/deeper"), writable);
    setPermissions(inner, QFile::ReadOwner | QFile::WriteOwner);
    setPermissions(top, QFile::ReadOwner | QFile::WriteOwner);
}

void TestPropertiesBatch::preservesPartiallyChangedBits()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeFile(dir.path(), QStringLiteral("file"), QByteArray("x"));
    QVERIFY(!path.isEmpty());
    setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                           | QFile::ReadGroup | QFile::ReadOther);

    PropertiesBatch::Change change;
    change.set = QFile::ReadOther;
    change.clear = QFile::WriteOwner;
    const QStringList failures = PropertiesBatch::apply({path}, change, false);
    QVERIFY(failures.isEmpty());

    const QFile::Permissions permissions = QFileInfo(path).permissions();
    QVERIFY(!(permissions & QFile::WriteOwner));
    // Bits the dialog showed as partially checked are left exactly as they were.
    QVERIFY(permissions & QFile::ReadOwner);
    QVERIFY(permissions & QFile::ExeOwner);
    QVERIFY(permissions & QFile::ReadGroup);
    QVERIFY(permissions & QFile::ReadOther);
}

QTEST_MAIN(TestPropertiesBatch)
#include "test_propertiesbatch.moc"
