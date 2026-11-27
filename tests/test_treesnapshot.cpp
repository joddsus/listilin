#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "treesnapshot.h"

namespace {

void write(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
    f.close();
}

} // namespace

class TestTreeSnapshot : public QObject
{
    Q_OBJECT
private slots:
    void completeTreeIsEqual();
    void missingEmptyFileIsDetected();
    void changedContentIsDetected();
    void changedSymlinkTargetIsDetected();
    void missingSymlinkIsDetected();
    void extraItemsAreAllowedForAMerge();
    void fingerprintChangesWithContent();
};

void TestTreeSnapshot::completeTreeIsEqual()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString first = tmp.path() + QStringLiteral("/a");
    const QString second = tmp.path() + QStringLiteral("/b");
    QVERIFY(QDir().mkpath(first + QStringLiteral("/sub")));
    QVERIFY(QDir().mkpath(second + QStringLiteral("/sub")));
    write(first + QStringLiteral("/sub/f.txt"), "hello");
    write(second + QStringLiteral("/sub/f.txt"), "hello");
    // The same literal link text in both trees: moving a relative link must not
    // rewrite it into an absolute path pointing at the old folder.
    const QString linkText = QStringLiteral("sub/f.txt");
    QVERIFY(QFile::link(linkText, first + QStringLiteral("/link")));
    QVERIFY(QFile::link(linkText, second + QStringLiteral("/link")));

    const TreeSnapshot::Tree a = TreeSnapshot::capture(first);
    const TreeSnapshot::Tree b = TreeSnapshot::capture(second);
    QVERIFY(a.complete);
    QVERIFY(b.complete);
    QVERIFY(TreeSnapshot::compare(a, b).isEmpty());
    QVERIFY(TreeSnapshot::missing(a, b).isEmpty());
}

void TestTreeSnapshot::missingEmptyFileIsDetected()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/source");
    const QString copy = tmp.path() + QStringLiteral("/copy");
    QVERIFY(QDir().mkpath(source));
    QVERIFY(QDir().mkpath(copy));
    write(source + QStringLiteral("/empty.txt"), QByteArray());
    write(source + QStringLiteral("/data.txt"), "data");
    write(copy + QStringLiteral("/data.txt"), "data");

    // Byte totals are identical, yet a file is missing.
    const QStringList differences
        = TreeSnapshot::missing(TreeSnapshot::capture(source), TreeSnapshot::capture(copy));
    QVERIFY(!differences.isEmpty());
}

void TestTreeSnapshot::changedContentIsDetected()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/source.txt");
    const QString copy = tmp.path() + QStringLiteral("/copy.txt");
    write(source, "AAAA");
    write(copy, "BBBB");

    const QStringList differences
        = TreeSnapshot::missing(TreeSnapshot::capture(source), TreeSnapshot::capture(copy));
    QVERIFY(!differences.isEmpty());
}

void TestTreeSnapshot::changedSymlinkTargetIsDetected()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/source");
    const QString copy = tmp.path() + QStringLiteral("/copy");
    QVERIFY(QDir().mkpath(source));
    QVERIFY(QDir().mkpath(copy));
    QVERIFY(QFile::link(QStringLiteral("/tmp/one"), source + QStringLiteral("/link")));
    QVERIFY(QFile::link(QStringLiteral("/tmp/two"), copy + QStringLiteral("/link")));

    const QStringList differences
        = TreeSnapshot::compare(TreeSnapshot::capture(source), TreeSnapshot::capture(copy));
    QVERIFY(!differences.isEmpty());
}

void TestTreeSnapshot::missingSymlinkIsDetected()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/source");
    const QString copy = tmp.path() + QStringLiteral("/copy");
    QVERIFY(QDir().mkpath(source));
    QVERIFY(QDir().mkpath(copy));
    write(source + QStringLiteral("/f.txt"), "data");
    write(copy + QStringLiteral("/f.txt"), "data");
    QVERIFY(QFile::link(QStringLiteral("f.txt"), source + QStringLiteral("/link")));

    const QStringList differences
        = TreeSnapshot::missing(TreeSnapshot::capture(source), TreeSnapshot::capture(copy));
    QVERIFY(!differences.isEmpty());
}

void TestTreeSnapshot::extraItemsAreAllowedForAMerge()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/source");
    const QString copy = tmp.path() + QStringLiteral("/copy");
    QVERIFY(QDir().mkpath(source));
    QVERIFY(QDir().mkpath(copy));
    write(source + QStringLiteral("/f.txt"), "data");
    write(copy + QStringLiteral("/f.txt"), "data");
    write(copy + QStringLiteral("/unrelated.txt"), "stays");

    QVERIFY(TreeSnapshot::missing(TreeSnapshot::capture(source),
                                  TreeSnapshot::capture(copy)).isEmpty());
    QVERIFY(!TreeSnapshot::compare(TreeSnapshot::capture(source),
                                   TreeSnapshot::capture(copy)).isEmpty());
}

void TestTreeSnapshot::fingerprintChangesWithContent()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QStringLiteral("/file.txt");
    write(path, "one");
    const QString first = TreeSnapshot::fingerprint(path);
    QVERIFY(!first.isEmpty());
    write(path, "two");
    QVERIFY(TreeSnapshot::fingerprint(path) != first);
}

QTEST_MAIN(TestTreeSnapshot)
#include "test_treesnapshot.moc"
