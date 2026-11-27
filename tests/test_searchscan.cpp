#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "searchscan.h"

class TestSearchScan : public QObject
{
    Q_OBJECT
private slots:
    void findsByNameRecursively();
    void filtersByExtensionSizeAndDate();
    void hiddenAndFoldersAreOptional();
    void doesNotFollowSymlinkedFolders();
    void searchesFileContents();
    void contentSearchIgnoresBinaryAndLargeFiles();
    void cancellationStopsEarly();
    void stopsAtTheResultLimit();
    void summaryExplainsWhyAListIsIncomplete();
};

namespace {

void write(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
    f.close();
}

} // namespace

void TestSearchScan::findsByNameRecursively()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir().mkpath(tmp.path() + QStringLiteral("/sub/deeper")));
    write(tmp.path() + QStringLiteral("/alpha.txt"), "a");
    write(tmp.path() + QStringLiteral("/sub/beta.txt"), "b");
    write(tmp.path() + QStringLiteral("/sub/don't-match.log"), "c");

    SearchScan::Query query;
    query.nameContains = QStringLiteral("MATCH");
    const SearchScan::Result result = SearchScan::run(tmp.path(), query);
    QCOMPARE(result.matches.size(), 1);
    QCOMPARE(result.matches.first(), tmp.path() + QStringLiteral("/sub/don't-match.log"));
    QVERIFY(!result.cancelled);
    QVERIFY(!result.truncated);
}

void TestSearchScan::filtersByExtensionSizeAndDate()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    write(tmp.path() + QStringLiteral("/small.txt"), QByteArray(10, 'a'));
    write(tmp.path() + QStringLiteral("/large.txt"), QByteArray(4096, 'a'));
    write(tmp.path() + QStringLiteral("/large.log"), QByteArray(4096, 'a'));

    SearchScan::Query query;
    query.extension = QStringLiteral(".txt");
    query.minBytes = 100;
    const SearchScan::Result result = SearchScan::run(tmp.path(), query);
    QCOMPARE(result.matches.size(), 1);
    QCOMPARE(result.matches.first(), tmp.path() + QStringLiteral("/large.txt"));

    SearchScan::Query future;
    future.modifiedAfter = QDateTime::currentDateTime().addDays(1);
    QVERIFY(SearchScan::run(tmp.path(), future).matches.isEmpty());
}

void TestSearchScan::hiddenAndFoldersAreOptional()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(QDir().mkpath(tmp.path() + QStringLiteral("/visible")));
    QVERIFY(QDir().mkpath(tmp.path() + QStringLiteral("/.hidden")));
    write(tmp.path() + QStringLiteral("/.hidden/inside.txt"), "x");
    write(tmp.path() + QStringLiteral("/visible/file.txt"), "x");

    SearchScan::Query filesOnly;
    filesOnly.includeHidden = true;
    const SearchScan::Result one = SearchScan::run(tmp.path(), filesOnly);
    QCOMPARE(one.matches.size(), 2);
    QVERIFY(one.matches.contains(tmp.path() + QStringLiteral("/.hidden/inside.txt")));

    SearchScan::Query folders;
    folders.includeFolders = true;
    const SearchScan::Result two = SearchScan::run(tmp.path(), folders);
    QVERIFY(two.matches.contains(tmp.path() + QStringLiteral("/visible")));
    QVERIFY(!two.matches.contains(tmp.path() + QStringLiteral("/.hidden")));
}

void TestSearchScan::doesNotFollowSymlinkedFolders()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString outside = tmp.path() + QStringLiteral("/outside");
    QVERIFY(QDir().mkpath(outside));
    write(outside + QStringLiteral("/hidden-away.txt"), "x");

    const QString root = tmp.path() + QStringLiteral("/root");
    QVERIFY(QDir().mkpath(root));
    QVERIFY(QFile::link(outside, root + QStringLiteral("/link")));

    const SearchScan::Result result = SearchScan::run(root, SearchScan::Query());
    QVERIFY(result.matches.isEmpty());
    QVERIFY(!result.matches.contains(outside + QStringLiteral("/hidden-away.txt")));
}

void TestSearchScan::searchesFileContents()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    write(tmp.path() + QStringLiteral("/notes.txt"),
          "Meeting notes\nWe need the lantern before Tuesday\n");
    write(tmp.path() + QStringLiteral("/other.txt"), "nothing relevant here\n");

    SearchScan::Query query;
    query.nameContains = QStringLiteral("LANTERN");
    query.searchContents = true;
    const SearchScan::Result result = SearchScan::run(tmp.path(), query);
    QCOMPARE(result.matches.size(), 1);
    QCOMPARE(result.matches.first(), tmp.path() + QStringLiteral("/notes.txt"));
    QCOMPARE(result.contentFilesRead, 2);
}

void TestSearchScan::contentSearchIgnoresBinaryAndLargeFiles()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QByteArray binary = QByteArray("needle", 6) + QByteArray(1, '\0') + QByteArray(100, 'x');
    write(tmp.path() + QStringLiteral("/image.bin"), binary);
    write(tmp.path() + QStringLiteral("/big.txt"), QByteArray(5000, 'n') + "needle");

    SearchScan::Query query;
    query.nameContains = QStringLiteral("needle");
    query.searchContents = true;
    query.maxContentBytes = 1000;
    const SearchScan::Result result = SearchScan::run(tmp.path(), query);
    QVERIFY(result.matches.isEmpty());
}

void TestSearchScan::cancellationStopsEarly()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    for (int i = 0; i < 50; ++i)
        write(tmp.path() + QStringLiteral("/file-%1.txt").arg(i), "x");

    int seen = 0;
    SearchScan::Callbacks callbacks;
    callbacks.cancelled = [&seen]() {
        ++seen;
        return seen > 3;
    };
    const SearchScan::Result result = SearchScan::run(tmp.path(), SearchScan::Query(), callbacks);
    QVERIFY(result.cancelled);
    QVERIFY(result.scannedEntries < 50);
    QVERIFY(result.matches.size() < 50);
}

void TestSearchScan::stopsAtTheResultLimit()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    for (int i = 0; i < 10; ++i)
        write(tmp.path() + QStringLiteral("/file-%1.txt").arg(i), "x");

    SearchScan::Query query;
    query.maximumResults = 3;
    const SearchScan::Result result = SearchScan::run(tmp.path(), query);
    QVERIFY(result.truncated);
    QCOMPARE(result.matches.size(), 3);
}

void TestSearchScan::summaryExplainsWhyAListIsIncomplete()
{
    SearchScan::Result truncated;
    truncated.truncated = true;
    truncated.matches = {QStringLiteral("/a"), QStringLiteral("/b")};
    QVERIFY(SearchScan::summary(truncated).contains(QStringLiteral("more")));

    SearchScan::Result cancelled;
    cancelled.cancelled = true;
    cancelled.matches = {QStringLiteral("/a")};
    QVERIFY(SearchScan::summary(cancelled).contains(QStringLiteral("cancelled")));

    SearchScan::Result complete;
    complete.matches = {QStringLiteral("/a")};
    QCOMPARE(SearchScan::summary(complete), QStringLiteral("1 matches"));

    SearchScan::Result unreadable;
    unreadable.unreadableFolders = {QStringLiteral("/secret")};
    QVERIFY(SearchScan::summary(unreadable).contains(QStringLiteral("/secret")));
}

QTEST_MAIN(TestSearchScan)
#include "test_searchscan.moc"
