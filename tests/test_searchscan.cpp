#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "searchscan.h"
#include "vfs.h"
#include "vfs_archive.h"
#include "vfs_local.h"

#include <memory>

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
    void vfsFindsMembersByNameAndFilters();
    void vfsIncludesFoldersAndStopsAtTheLimit();
    void vfsReportsAnUnreadableLocationAndCancels();
    void vfsSaysContentSearchWasSkipped();
    void initTestCase();
};

namespace {

void write(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
    f.close();
}

void registerBackends()
{
    if (!Vfs::instance().backendFor(QStringLiteral("file")))
        Vfs::instance().addBackend(std::make_shared<LocalBackend>());
    if (!Vfs::instance().backendFor(QStringLiteral("archive")))
        Vfs::instance().addBackend(std::make_shared<ArchiveBackend>());
}

} // namespace

void TestSearchScan::initTestCase()
{
    registerBackends();
}

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

void TestSearchScan::vfsFindsMembersByNameAndFilters()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/search.zip");
    const QString root = VfsUri::archiveUriFor(QStringLiteral("file://") + archivePath);

    const QString one = tmp.path() + QStringLiteral("/one.txt");
    write(one, QByteArray("short"));
    const QString two = tmp.path() + QStringLiteral("/two.txt");
    write(two, QByteArray(2048, 'x'));
    const QString three = tmp.path() + QStringLiteral("/three.log");
    write(three, QByteArray(2048, 'y'));

    QString error;
    QVERIFY2(Vfs::instance().put(root, QStringLiteral("one.txt"), one, &error), qPrintable(error));
    QVERIFY2(Vfs::instance().put(root, QStringLiteral("two.txt"), two, &error), qPrintable(error));
    QVERIFY2(Vfs::instance().put(root, QStringLiteral("three.log"), three, &error),
             qPrintable(error));
    QVERIFY2(Vfs::instance().mkdir(root, QStringLiteral("sub"), &error), qPrintable(error));
    const QString subUri = VfsUri::childUri(root, QStringLiteral("sub"));
    QVERIFY2(Vfs::instance().put(subUri, QStringLiteral("nested.txt"), two, &error),
             qPrintable(error));

    SearchScan::Query byName;
    byName.nameContains = QStringLiteral("NESTED");
    SearchScan::Result result = SearchScan::runVfs(root, byName);
    QCOMPARE(result.matches.size(), 1);
    QCOMPARE(result.matches.first(), VfsUri::childUri(subUri, QStringLiteral("nested.txt")));
    QVERIFY(result.unreadableFolders.isEmpty());

    SearchScan::Query byFilter;
    byFilter.extension = QStringLiteral(".txt");
    byFilter.minBytes = 1000;
    result = SearchScan::runVfs(root, byFilter);
    QCOMPARE(result.matches.size(), 2);
    QVERIFY(result.matches.contains(VfsUri::childUri(root, QStringLiteral("two.txt"))));
    QVERIFY(result.matches.contains(VfsUri::childUri(subUri, QStringLiteral("nested.txt"))));
    QVERIFY(!result.matches.contains(VfsUri::childUri(root, QStringLiteral("one.txt"))));
}

void TestSearchScan::vfsIncludesFoldersAndStopsAtTheLimit()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/folders.zip");
    const QString root = VfsUri::archiveUriFor(QStringLiteral("file://") + archivePath);

    const QString payload = tmp.path() + QStringLiteral("/payload.txt");
    write(payload, "payload");
    QString error;
    QVERIFY(Vfs::instance().mkdir(root, QStringLiteral("keep-folder"), &error));
    for (int i = 0; i < 6; ++i) {
        QVERIFY2(Vfs::instance().put(root, QStringLiteral("match-%1.txt").arg(i), payload, &error),
                 qPrintable(error));
    }

    SearchScan::Query folders;
    folders.nameContains = QStringLiteral("keep");
    folders.includeFolders = true;
    SearchScan::Result result = SearchScan::runVfs(root, folders);
    QCOMPARE(result.matches.size(), 1);
    QCOMPARE(result.matches.first(), VfsUri::childUri(root, QStringLiteral("keep-folder")));

    SearchScan::Query many;
    many.nameContains = QStringLiteral("match");
    many.maximumResults = 2;
    result = SearchScan::runVfs(root, many);
    QVERIFY(result.truncated);
    QCOMPARE(result.matches.size(), 2);
}

void TestSearchScan::vfsReportsAnUnreadableLocationAndCancels()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString broken = tmp.path() + QStringLiteral("/broken.zip");
    write(broken, "this is not an archive at all");
    const QString brokenUri = VfsUri::archiveUriFor(QStringLiteral("file://") + broken);

    SearchScan::Result result = SearchScan::runVfs(brokenUri, SearchScan::Query());
    QVERIFY(result.matches.isEmpty());
    QCOMPARE(result.unreadableFolders.size(), 1);
    QCOMPARE(result.unreadableFolders.first(), brokenUri);
    QVERIFY(SearchScan::summary(result).contains(QStringLiteral("could not be read")));

    const QString archivePath = tmp.path() + QStringLiteral("/cancel.zip");
    const QString root = VfsUri::archiveUriFor(QStringLiteral("file://") + archivePath);
    const QString payload = tmp.path() + QStringLiteral("/thing.txt");
    write(payload, "thing");
    QString error;
    for (int i = 0; i < 30; ++i) {
        QVERIFY2(Vfs::instance().put(root, QStringLiteral("item-%1.txt").arg(i), payload, &error),
                 qPrintable(error));
    }

    int checks = 0;
    SearchScan::Callbacks callbacks;
    callbacks.cancelled = [&checks]() {
        ++checks;
        return checks > 3;
    };
    result = SearchScan::runVfs(root, SearchScan::Query(), callbacks);
    QVERIFY(result.cancelled);
    QVERIFY(result.scannedEntries < 30);
}

void TestSearchScan::vfsSaysContentSearchWasSkipped()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/content.zip");
    const QString root = VfsUri::archiveUriFor(QStringLiteral("file://") + archivePath);
    const QString payload = tmp.path() + QStringLiteral("/note.txt");
    write(payload, "the lantern is here");
    QString error;
    QVERIFY(Vfs::instance().put(root, QStringLiteral("note.txt"), payload, &error));

    SearchScan::Query query;
    query.nameContains = QStringLiteral("lantern");
    query.searchContents = true;
    const SearchScan::Result result = SearchScan::runVfs(root, query);
    QVERIFY(result.matches.isEmpty());
    QVERIFY(result.contentSearchSkipped);
    QVERIFY(SearchScan::summary(result).contains(QStringLiteral("local files only")));
}

QTEST_MAIN(TestSearchScan)
#include "test_searchscan.moc"
