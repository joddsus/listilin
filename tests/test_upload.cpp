#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>

#include <memory>

#include "fileops.h"
#include "vfs.h"
#include "vfs_archive.h"
#include "vfs_local.h"

// A backend that claims every upload succeeded but stores only half of the
// bytes. It exists to prove that uploads are read back and compared instead of
// trusted.
class LyingBackend : public VfsBackend
{
public:
    QString scheme() const override { return QStringLiteral("fake"); }
    bool writable() const override { return true; }

    bool list(const VfsUri &uri, bool, QVector<Entry> *entries, QString *) override
    {
        if (!entries)
            return false;
        entries->clear();
        const QString prefix = uri.toString();
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            const QString key = it.key();
            if (!key.startsWith(prefix))
                continue;
            QString rest = key.mid(prefix.size());
            if (rest.startsWith(QLatin1Char('/')))
                rest.remove(0, 1);
            if (rest.isEmpty() || rest.contains(QLatin1Char('/')))
                continue;
            Entry entry;
            entry.name = rest;
            entry.isDir = false;
            entry.size = it.value().size();
            entries->append(entry);
        }
        for (const QString &dir : dirs) {
            if (!dir.startsWith(prefix))
                continue;
            QString rest = dir.mid(prefix.size());
            if (rest.startsWith(QLatin1Char('/')))
                rest.remove(0, 1);
            if (rest.isEmpty() || rest.contains(QLatin1Char('/')))
                continue;
            Entry entry;
            entry.name = rest;
            entry.isDir = true;
            entries->append(entry);
        }
        return true;
    }

    bool fetch(const VfsUri &uri, const QString &target, QString *) override
    {
        const auto it = files.constFind(uri.toString());
        if (it == files.constEnd())
            return false;
        QFile out(target);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        out.write(it.value());
        return true;
    }

    bool put(const VfsUri &targetDir, const QString &name, const QString &localFile,
             QString *) override
    {
        QFile in(localFile);
        if (!in.open(QIODevice::ReadOnly))
            return false;
        QByteArray data = in.readAll();
        // The lie: everything is cut in half.
        data.truncate(data.size() / 2);
        files.insert(VfsUri::childUri(targetDir.toString(), name), data);
        return true;
    }

    bool mkdir(const VfsUri &targetDir, const QString &name, QString *) override
    {
        dirs.insert(VfsUri::childUri(targetDir.toString(), name));
        return true;
    }

    QHash<QString, QByteArray> files;
    QSet<QString> dirs;
};

class TestUpload : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void verifiedArchiveUpload();
    void moveDeletesLocalSourceOnlyAfterVerification();
    void corruptUploadKeepsSourceAndFails();
    void conflictSkipLeavesExistingMember();
    void folderUploadsRecursivelyAndKeepsEmptyFolders();
    void linksAreRejectedNotFollowed();
    void readOnlyTargetFailsWithoutTouchingAnything();
};

static void registerBackends()
{
    if (!Vfs::instance().backendFor(QStringLiteral("file")))
        Vfs::instance().addBackend(std::make_shared<LocalBackend>());
    if (!Vfs::instance().backendFor(QStringLiteral("archive")))
        Vfs::instance().addBackend(std::make_shared<ArchiveBackend>());
}

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(f.write(data), qint64(data.size()));
    f.close();
}

static QByteArray readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

static QByteArray fetchMember(const QString &archiveUri, const QString &member)
{
    const QString uri = archiveUri + QStringLiteral("!/") + member;
    QString error;
    QTemporaryFile out;
    if (!out.open())
        return QByteArray();
    const QString path = out.fileName();
    out.close();
    if (!Vfs::instance().fetch(uri, path, &error))
        return QByteArray::fromStdString(error.toStdString());
    return readFile(path);
}

void TestUpload::initTestCase()
{
    registerBackends();
}

void TestUpload::verifiedArchiveUpload()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/dest.zip");
    const QByteArray payload("the contents that must arrive intact");
    const QString source = tmp.path() + QStringLiteral("/notes.txt");
    writeFile(source, payload);

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + source;
    item.isDir = false;

    QString error;
    const FileOps::Result result = FileOps::uploadUrisResult(
        {item}, VfsUri::archiveUriFor(QStringLiteral("file://") + archivePath), false, &error,
        FileOps::Progress());
    QVERIFY2(result.failed.isEmpty(), qPrintable(error));
    QCOMPARE(result.itemsCopied, 1);
    QVERIFY(QFile::exists(source));
    QCOMPARE(fetchMember(VfsUri::archiveUriFor(QStringLiteral("file://") + archivePath),
                         QStringLiteral("notes.txt")),
             payload);
}

void TestUpload::moveDeletesLocalSourceOnlyAfterVerification()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/move.zip");
    const QString source = tmp.path() + QStringLiteral("/moved.txt");
    writeFile(source, "move me");

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + source;
    item.isDir = false;

    QString error;
    const FileOps::Result result = FileOps::uploadUrisResult(
        {item}, VfsUri::archiveUriFor(QStringLiteral("file://") + archivePath), true, &error,
        FileOps::Progress());
    QVERIFY2(result.failed.isEmpty(), qPrintable(error));
    QVERIFY(!QFile::exists(source));
    QCOMPARE(fetchMember(VfsUri::archiveUriFor(QStringLiteral("file://") + archivePath),
                         QStringLiteral("moved.txt")),
             QByteArray("move me"));
}

void TestUpload::corruptUploadKeepsSourceAndFails()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/payload.bin");
    const QByteArray payload(4096, 'q');
    writeFile(source, payload);

    auto backend = std::make_shared<LyingBackend>();
    Vfs::instance().addBackend(backend);

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + source;
    item.isDir = false;

    QString error;
    const FileOps::Result result = FileOps::uploadUrisResult(
        {item}, QStringLiteral("fake://host/folder"), true, &error, FileOps::Progress());

    QCOMPARE(result.failed.size(), 1);
    QVERIFY(!error.isEmpty());
    QVERIFY2(error.contains(QStringLiteral("read back"))
                 || error.contains(QStringLiteral("does not match")),
             qPrintable(error));
    QVERIFY(QFile::exists(source));
    QCOMPARE(readFile(source), payload);
}

void TestUpload::conflictSkipLeavesExistingMember()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archivePath = tmp.path() + QStringLiteral("/conflict.zip");
    const QString archiveUri = VfsUri::archiveUriFor(QStringLiteral("file://") + archivePath);

    const QString oldSource = tmp.path() + QStringLiteral("/old.txt");
    writeFile(oldSource, "old contents");
    FileOps::CopyItem oldItem;
    oldItem.uri = QStringLiteral("file://") + oldSource;
    QString error;
    QVERIFY(FileOps::uploadUrisResult({oldItem}, archiveUri, false, &error, FileOps::Progress())
                .failed.isEmpty());

    QVERIFY(QDir().mkpath(tmp.path() + QStringLiteral("/other")));
    const QString newSource = tmp.path() + QStringLiteral("/other/old.txt");
    writeFile(newSource, "new contents");

    FileOps::Progress progress;
    progress.conflict = [](const QString &, const QString &) { return FileOps::ConflictSkip; };

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + newSource;
    const FileOps::Result result
        = FileOps::uploadUrisResult({item}, archiveUri, false, &error, progress);
    QVERIFY2(result.failed.isEmpty(), qPrintable(error));
    QCOMPARE(result.itemsSkipped, 1);
    QCOMPARE(fetchMember(archiveUri, QStringLiteral("old.txt")), QByteArray("old contents"));
}

void TestUpload::folderUploadsRecursivelyAndKeepsEmptyFolders()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString root = tmp.path() + QStringLiteral("/tree");
    QVERIFY(QDir().mkpath(root + QStringLiteral("/sub/empty")));
    writeFile(root + QStringLiteral("/top.txt"), "top");
    writeFile(root + QStringLiteral("/sub/inner.txt"), "inner");

    const QString archivePath = tmp.path() + QStringLiteral("/tree.zip");
    const QString archiveUri = VfsUri::archiveUriFor(QStringLiteral("file://") + archivePath);

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + root;
    item.isDir = true;

    QString error;
    const FileOps::Result result
        = FileOps::uploadUrisResult({item}, archiveUri, false, &error, FileOps::Progress());
    QVERIFY2(result.failed.isEmpty(), qPrintable(error));
    QCOMPARE(result.itemsCopied, 2);
    QCOMPARE(fetchMember(archiveUri, QStringLiteral("tree/top.txt")), QByteArray("top"));
    QCOMPARE(fetchMember(archiveUri, QStringLiteral("tree/sub/inner.txt")),
             QByteArray("inner"));

    QVector<Entry> entries;
    QVERIFY(Vfs::instance().list(archiveUri + QStringLiteral("!/tree/sub"), false, &entries,
                                 &error));
    bool sawEmpty = false;
    for (const Entry &entry : entries)
        sawEmpty = sawEmpty || (entry.name == QStringLiteral("empty") && entry.isDir);
    QVERIFY(sawEmpty);
}

void TestUpload::linksAreRejectedNotFollowed()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString real = tmp.path() + QStringLiteral("/real.txt");
    writeFile(real, "target");
    const QString link = tmp.path() + QStringLiteral("/link.txt");
    QVERIFY(QFile::link(real, link));

    const QString archiveUri
        = VfsUri::archiveUriFor(QStringLiteral("file://") + tmp.path() + QStringLiteral("/a.zip"));

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + link;

    QString error;
    const FileOps::Result result
        = FileOps::uploadUrisResult({item}, archiveUri, false, &error, FileOps::Progress());
    QCOMPARE(result.failed.size(), 1);
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFile::exists(tmp.path() + QStringLiteral("/a.zip")));
}

void TestUpload::readOnlyTargetFailsWithoutTouchingAnything()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/keep.txt");
    writeFile(source, "keep");

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + source;

    QString error;
    const FileOps::Result result = FileOps::uploadUrisResult(
        {item}, QStringLiteral("unknown://host/folder"), true, &error, FileOps::Progress());
    QCOMPARE(result.failed.size(), 1);
    QVERIFY(!error.isEmpty());
    QVERIFY(QFile::exists(source));
}

QTEST_MAIN(TestUpload)
#include "test_upload.moc"
