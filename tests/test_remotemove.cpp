#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>

#include <memory>

#include "fileops.h"
#include "vfs.h"
#include "vfs_local.h"

// An in-memory archive/server that can hold folders as well as files, so a move
// between two of them can be driven end to end without a real network server.
// The failure switches let a test make one end misbehave the way a real backend
// can (a truncated download, a refused upload, a refused delete).
class MemoryBackend : public VfsBackend
{
public:
    explicit MemoryBackend(QString name)
        : m_scheme(std::move(name))
    {
    }

    QString scheme() const override { return m_scheme; }
    bool writable() const override { return true; }

    bool list(const VfsUri &uri, bool, QVector<Entry> *entries, QString *) override
    {
        entries->clear();
        const QString prefix = uri.toString();
        QHash<QString, bool> children; // name -> isDir
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            if (!it.key().startsWith(prefix + QLatin1Char('/')))
                continue;
            QString rest = it.key().mid(prefix.size() + 1);
            const int slash = rest.indexOf(QLatin1Char('/'));
            const QString name = slash < 0 ? rest : rest.left(slash);
            if (name.isEmpty())
                continue;
            const bool nested = slash >= 0;
            children[name] = children.value(name, false) || nested;
        }
        for (const QString &dir : dirs) {
            if (!dir.startsWith(prefix + QLatin1Char('/')))
                continue;
            QString rest = dir.mid(prefix.size() + 1);
            if (rest.isEmpty())
                continue;
            const int slash = rest.indexOf(QLatin1Char('/'));
            const QString name = slash < 0 ? rest : rest.left(slash);
            children[name] = true;
        }

        for (auto it = children.constBegin(); it != children.constEnd(); ++it) {
            Entry entry;
            entry.name = it.key();
            entry.isDir = it.value();
            if (!entry.isDir) {
                const QByteArray data
                    = files.value(VfsUri::childUri(prefix, entry.name));
                entry.size = data.size();
                entry.hasSize = true;
            }
            entries->append(entry);
        }
        return true;
    }

    bool fetch(const VfsUri &uri, const QString &target, QString *error) override
    {
        const auto it = files.constFind(uri.toString());
        if (it == files.constEnd()) {
            if (error)
                *error = QStringLiteral("%1 is not on this server").arg(vfsLeafName(uri.toString()));
            return false;
        }
        QFile out(target);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        const QByteArray payload
            = truncateOnFetch ? it.value().left(it.value().size() / 2) : it.value();
        out.write(payload);
        return true;
    }

    bool put(const VfsUri &targetDir, const QString &name, const QString &localFile,
             QString *error) override
    {
        if (refusePut) {
            if (error)
                *error = QStringLiteral("This server refuses uploads.");
            return false;
        }
        QFile in(localFile);
        if (!in.open(QIODevice::ReadOnly))
            return false;
        QByteArray payload = in.readAll();
        if (truncateOnPut)
            payload = payload.left(payload.size() / 2);
        files.insert(VfsUri::childUri(targetDir.toString(), name), payload);
        return true;
    }

    bool mkdir(const VfsUri &targetDir, const QString &name, QString *error) override
    {
        if (refusePut) {
            if (error)
                *error = QStringLiteral("This server refuses changes.");
            return false;
        }
        dirs.insert(VfsUri::childUri(targetDir.toString(), name));
        return true;
    }

    bool remove(const VfsUri &uri, bool recursive, QString *error) override
    {
        if (refuseRemove) {
            if (error)
                *error = QStringLiteral("This server does not allow deletes.");
            return false;
        }
        const QString key = uri.toString();
        if (files.remove(key) > 0)
            return true;

        QSet<QString> nested;
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            if (it.key().startsWith(key + QLatin1Char('/')))
                nested.insert(it.key());
        }
        QSet<QString> nestedDirs;
        for (const QString &dir : dirs) {
            if (dir.startsWith(key + QLatin1Char('/')))
                nestedDirs.insert(dir);
        }
        if (nested.isEmpty() && nestedDirs.isEmpty() && !dirs.contains(key)) {
            if (error)
                *error = QStringLiteral("%1 is not on this server").arg(vfsLeafName(key));
            return false;
        }
        if (!recursive && !nested.isEmpty()) {
            if (error)
                *error = QStringLiteral("%1 is not empty").arg(vfsLeafName(key));
            return false;
        }
        for (const QString &entry : nested)
            files.remove(entry);
        for (const QString &dir : nestedDirs)
            dirs.remove(dir);
        dirs.remove(key);
        return true;
    }

    QHash<QString, QByteArray> files;
    QSet<QString> dirs;
    bool truncateOnFetch = false;
    bool truncateOnPut = false;
    bool refusePut = false;
    bool refuseRemove = false;

private:
    QString m_scheme;
};

class TestRemoteMove : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void movesAFileBetweenServers();
    void movesAFolderIncludingAnEmptyFolder();
    void keepsTheSourceWhenTheDownloadIsTruncated();
    void keepsTheSourceWhenTheUploadIsTruncated();
    void keepsTheSourceWhenTheDestinationRefusesTheUpload();
    void reportsACopyWhenTheSourceRefusesTheDelete();
    void refusesAFolderThatIsAlreadyTheTarget();
    void refusesALocalSource();
    void undoPutsTheFileBackAndRemovesTheDestinationCopy();
    void undoRefusesWhenTheCopyChangedAtTheDestination();
    void undoRefusesWhenTheOriginalNameIsTaken();
    void undoPutsAFolderBack();
};

static void registerBackends()
{
    if (!Vfs::instance().backendFor(QStringLiteral("file")))
        Vfs::instance().addBackend(std::make_shared<LocalBackend>());
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

static QStringList namesOf(const QString &uri)
{
    QVector<Entry> entries;
    QString error;
    if (!Vfs::instance().list(uri, true, &entries, &error))
        return {QStringLiteral("<list failed: ") + error + QLatin1Char('>')};
    QStringList names;
    for (const Entry &entry : entries)
        names.append(entry.name);
    names.sort();
    return names;
}

static std::shared_ptr<MemoryBackend> addServer(const QString &scheme)
{
    auto backend = std::make_shared<MemoryBackend>(scheme);
    Vfs::instance().addBackend(backend);
    return backend;
}

// Puts a file on a memory backend from a temporary local source.
static void seedFile(const std::shared_ptr<MemoryBackend> &server, const QString &root,
                     const QString &name, const QByteArray &contents)
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QLatin1Char('/') + name;
    writeFile(source, contents);
    QString error;
    QVERIFY2(Vfs::instance().put(root, name, source, &error), qPrintable(error));
    Q_UNUSED(server)
}

void TestRemoteMove::initTestCase()
{
    registerBackends();
}

void TestRemoteMove::movesAFileBetweenServers()
{
    auto source = addServer(QStringLiteral("movesrc"));
    auto destination = addServer(QStringLiteral("movedst"));
    const QString sourceRoot = QStringLiteral("movesrc://host/src");
    const QString targetRoot = QStringLiteral("movedst://host/dst");

    seedFile(source, sourceRoot, QStringLiteral("report.txt"), "quarterly numbers");

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(sourceRoot, QStringLiteral("report.txt"));
    QString error;
    const FileOps::Result result
        = FileOps::moveRemoteToRemoteResult({item}, targetRoot, &error, FileOps::Progress());

    QVERIFY2(result.failed.isEmpty(), qPrintable(error));
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.itemsCopied, 1);
    QVERIFY(namesOf(sourceRoot).isEmpty());
    QCOMPARE(namesOf(targetRoot), QStringList{QStringLiteral("report.txt")});
    QCOMPARE(destination->files.value(VfsUri::childUri(targetRoot, QStringLiteral("report.txt"))),
             QByteArray("quarterly numbers"));

    QCOMPARE(result.movesRemote.size(), 1);
    const UploadRecord::RemoteMoveItem &record = result.movesRemote.first();
    QCOMPARE(record.sourceParentUri, sourceRoot);
    QCOMPARE(record.sourceName, QStringLiteral("report.txt"));
    QCOMPARE(record.destinationUri,
             VfsUri::childUri(targetRoot, QStringLiteral("report.txt")));
    QVERIFY(!record.digest.isEmpty());
    QVERIFY(!record.isDir);
}

void TestRemoteMove::movesAFolderIncludingAnEmptyFolder()
{
    addServer(QStringLiteral("foldersrc"));
    auto destination = addServer(QStringLiteral("folderdst"));
    const QString sourceRoot = QStringLiteral("foldersrc://host/src");
    const QString targetRoot = QStringLiteral("folderdst://host/dst");

    QString error;
    const QString tree = VfsUri::childUri(sourceRoot, QStringLiteral("tree"));
    QVERIFY(Vfs::instance().mkdir(sourceRoot, QStringLiteral("tree"), &error));
    QVERIFY(Vfs::instance().mkdir(tree, QStringLiteral("empty"), &error));
    seedFile(nullptr, tree, QStringLiteral("inner.txt"), "inner");

    FileOps::CopyItem item;
    item.uri = tree;
    item.isDir = true;
    const FileOps::Result result
        = FileOps::moveRemoteToRemoteResult({item}, targetRoot, &error, FileOps::Progress());

    QVERIFY2(result.failed.isEmpty(), qPrintable(error));
    QCOMPARE(result.movesRemote.size(), 1);
    QVERIFY(result.movesRemote.first().isDir);
    QVERIFY(result.movesRemote.first().digest.startsWith(QStringLiteral("tree:")));

    QVERIFY(namesOf(sourceRoot).isEmpty());
    QCOMPARE(namesOf(targetRoot), QStringList{QStringLiteral("tree")});
    QCOMPARE(namesOf(VfsUri::childUri(targetRoot, QStringLiteral("tree"))),
             (QStringList{QStringLiteral("empty"), QStringLiteral("inner.txt")}));
    QVERIFY(destination->dirs.contains(
        VfsUri::childUri(targetRoot, QStringLiteral("tree/empty"))));
    QCOMPARE(destination->files.value(
                 VfsUri::childUri(targetRoot, QStringLiteral("tree/inner.txt"))),
             QByteArray("inner"));
}

void TestRemoteMove::keepsTheSourceWhenTheDownloadIsTruncated()
{
    auto source = addServer(QStringLiteral("cutsrc"));
    addServer(QStringLiteral("cutdst"));
    source->truncateOnFetch = true;
    const QString sourceRoot = QStringLiteral("cutsrc://host/src");
    const QString targetRoot = QStringLiteral("cutdst://host/dst");

    seedFile(source, sourceRoot, QStringLiteral("big.bin"), QByteArray(4000, 'x'));

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(sourceRoot, QStringLiteral("big.bin"));
    QString error;
    const FileOps::Result result
        = FileOps::moveRemoteToRemoteResult({item}, targetRoot, &error, FileOps::Progress());

    QCOMPARE(result.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("did not arrive intact")), qPrintable(error));
    QVERIFY(result.movesRemote.isEmpty());
    QCOMPARE(source->files.value(item.uri).size(), 4000);
    QVERIFY(namesOf(targetRoot).isEmpty());
}

void TestRemoteMove::keepsTheSourceWhenTheUploadIsTruncated()
{
    addServer(QStringLiteral("putsrc"));
    auto destination = addServer(QStringLiteral("putdst"));
    destination->truncateOnPut = true;
    const QString sourceRoot = QStringLiteral("putsrc://host/src");
    const QString targetRoot = QStringLiteral("putdst://host/dst");

    seedFile(nullptr, sourceRoot, QStringLiteral("data.txt"), "the whole file");

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(sourceRoot, QStringLiteral("data.txt"));
    QString error;
    const FileOps::Result result
        = FileOps::moveRemoteToRemoteResult({item}, targetRoot, &error, FileOps::Progress());

    QCOMPARE(result.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("does not match the source")), qPrintable(error));
    QVERIFY(result.movesRemote.isEmpty());
    QCOMPARE(namesOf(sourceRoot), QStringList{QStringLiteral("data.txt")});
}

void TestRemoteMove::keepsTheSourceWhenTheDestinationRefusesTheUpload()
{
    auto source = addServer(QStringLiteral("nosrc"));
    auto destination = addServer(QStringLiteral("nodst"));
    destination->refusePut = true;
    const QString sourceRoot = QStringLiteral("nosrc://host/src");
    const QString targetRoot = QStringLiteral("nodst://host/dst");

    seedFile(source, sourceRoot, QStringLiteral("file.txt"), "contents");

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(sourceRoot, QStringLiteral("file.txt"));
    QString error;
    const FileOps::Result result
        = FileOps::moveRemoteToRemoteResult({item}, targetRoot, &error, FileOps::Progress());

    QCOMPARE(result.failed.size(), 1);
    QVERIFY(result.movesRemote.isEmpty());
    QCOMPARE(namesOf(sourceRoot), QStringList{QStringLiteral("file.txt")});
    QVERIFY(namesOf(targetRoot).isEmpty());
}

void TestRemoteMove::reportsACopyWhenTheSourceRefusesTheDelete()
{
    auto source = addServer(QStringLiteral("locked"));
    auto destination = addServer(QStringLiteral("free"));
    source->refuseRemove = true;
    const QString sourceRoot = QStringLiteral("locked://host/src");
    const QString targetRoot = QStringLiteral("free://host/dst");

    seedFile(source, sourceRoot, QStringLiteral("stay.txt"), "copied but kept");

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(sourceRoot, QStringLiteral("stay.txt"));
    QString error;
    const FileOps::Result result
        = FileOps::moveRemoteToRemoteResult({item}, targetRoot, &error, FileOps::Progress());

    QCOMPARE(result.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("could not be removed")), qPrintable(error));
    QVERIFY(result.movesRemote.isEmpty());
    // Both copies survive; nothing claims the move finished.
    QCOMPARE(namesOf(sourceRoot), QStringList{QStringLiteral("stay.txt")});
    QCOMPARE(destination->files.value(
                 VfsUri::childUri(targetRoot, QStringLiteral("stay.txt"))),
             QByteArray("copied but kept"));
    QCOMPARE(result.itemsCopied, 0);
}

void TestRemoteMove::refusesAFolderThatIsAlreadyTheTarget()
{
    auto source = addServer(QStringLiteral("samesrv"));
    const QString root = QStringLiteral("samesrv://host/folder");
    seedFile(source, root, QStringLiteral("here.txt"), "already here");

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(root, QStringLiteral("here.txt"));
    QString error;
    const FileOps::Result result
        = FileOps::moveRemoteToRemoteResult({item}, root, &error, FileOps::Progress());

    QCOMPARE(result.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("already in that folder")), qPrintable(error));
    QCOMPARE(namesOf(root), QStringList{QStringLiteral("here.txt")});
    QCOMPARE(source->files.value(item.uri), QByteArray("already here"));
}

void TestRemoteMove::refusesALocalSource()
{
    addServer(QStringLiteral("targetonly"));
    const QString targetRoot = QStringLiteral("targetonly://host/dst");
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString local = tmp.path() + QStringLiteral("/local.txt");
    writeFile(local, "local");

    FileOps::CopyItem item;
    item.uri = QStringLiteral("file://") + local;
    QString error;
    const FileOps::Result result
        = FileOps::moveRemoteToRemoteResult({item}, targetRoot, &error, FileOps::Progress());

    QCOMPARE(result.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("local entry")), qPrintable(error));
    QVERIFY(QFile::exists(local));
    QVERIFY(namesOf(targetRoot).isEmpty());
}

void TestRemoteMove::undoPutsTheFileBackAndRemovesTheDestinationCopy()
{
    auto source = addServer(QStringLiteral("undosrc"));
    auto destination = addServer(QStringLiteral("undodst"));
    const QString sourceRoot = QStringLiteral("undosrc://host/src");
    const QString targetRoot = QStringLiteral("undodst://host/dst");

    seedFile(source, sourceRoot, QStringLiteral("letter.txt"), "dear listi");

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(sourceRoot, QStringLiteral("letter.txt"));
    QString error;
    const FileOps::Result moved
        = FileOps::moveRemoteToRemoteResult({item}, targetRoot, &error, FileOps::Progress());
    QVERIFY2(moved.failed.isEmpty(), qPrintable(error));
    QCOMPARE(moved.movesRemote.size(), 1);

    const FileOps::Result undo
        = FileOps::undoRemoteMovesResult(moved.movesRemote, &error, FileOps::Progress());

    QVERIFY2(undo.failed.isEmpty(), qPrintable(error));
    QVERIFY2(undo.error.isEmpty(), qPrintable(undo.error));
    QCOMPARE(namesOf(sourceRoot), QStringList{QStringLiteral("letter.txt")});
    QCOMPARE(source->files.value(item.uri), QByteArray("dear listi"));
    QVERIFY(namesOf(targetRoot).isEmpty());
}

void TestRemoteMove::undoRefusesWhenTheCopyChangedAtTheDestination()
{
    addServer(QStringLiteral("editsrc"));
    auto destination = addServer(QStringLiteral("editdst"));
    const QString sourceRoot = QStringLiteral("editsrc://host/src");
    const QString targetRoot = QStringLiteral("editdst://host/dst");

    seedFile(nullptr, sourceRoot, QStringLiteral("note.txt"), "original");

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(sourceRoot, QStringLiteral("note.txt"));
    QString error;
    const FileOps::Result moved
        = FileOps::moveRemoteToRemoteResult({item}, targetRoot, &error, FileOps::Progress());
    QVERIFY2(moved.failed.isEmpty(), qPrintable(error));

    const QString movedUri = VfsUri::childUri(targetRoot, QStringLiteral("note.txt"));
    destination->files.insert(movedUri, "edited at the destination");

    const FileOps::Result undo
        = FileOps::undoRemoteMovesResult(moved.movesRemote, &error, FileOps::Progress());

    QCOMPARE(undo.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("changed")), qPrintable(error));
    QCOMPARE(destination->files.value(movedUri), QByteArray("edited at the destination"));
    QVERIFY(namesOf(sourceRoot).isEmpty());
}

void TestRemoteMove::undoRefusesWhenTheOriginalNameIsTaken()
{
    addServer(QStringLiteral("takensrc"));
    addServer(QStringLiteral("takendst"));
    const QString sourceRoot = QStringLiteral("takensrc://host/src");
    const QString targetRoot = QStringLiteral("takendst://host/dst");

    seedFile(nullptr, sourceRoot, QStringLiteral("shared.txt"), "the original");

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(sourceRoot, QStringLiteral("shared.txt"));
    QString error;
    const FileOps::Result moved
        = FileOps::moveRemoteToRemoteResult({item}, targetRoot, &error, FileOps::Progress());
    QVERIFY2(moved.failed.isEmpty(), qPrintable(error));

    seedFile(nullptr, sourceRoot, QStringLiteral("shared.txt"), "somebody else's file");

    const FileOps::Result undo
        = FileOps::undoRemoteMovesResult(moved.movesRemote, &error, FileOps::Progress());

    QCOMPARE(undo.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("already exists")), qPrintable(error));
    QCOMPARE(namesOf(targetRoot), QStringList{QStringLiteral("shared.txt")});
}

void TestRemoteMove::undoPutsAFolderBack()
{
    addServer(QStringLiteral("dirsrc"));
    auto destination = addServer(QStringLiteral("dirdst"));
    const QString sourceRoot = QStringLiteral("dirsrc://host/src");
    const QString targetRoot = QStringLiteral("dirdst://host/dst");

    QString error;
    const QString tree = VfsUri::childUri(sourceRoot, QStringLiteral("tree"));
    QVERIFY(Vfs::instance().mkdir(sourceRoot, QStringLiteral("tree"), &error));
    QVERIFY(Vfs::instance().mkdir(tree, QStringLiteral("empty"), &error));
    seedFile(nullptr, tree, QStringLiteral("inner.txt"), "inner");

    FileOps::CopyItem item;
    item.uri = tree;
    item.isDir = true;
    const FileOps::Result moved
        = FileOps::moveRemoteToRemoteResult({item}, targetRoot, &error, FileOps::Progress());
    QVERIFY2(moved.failed.isEmpty(), qPrintable(error));
    QCOMPARE(moved.movesRemote.size(), 1);

    const FileOps::Result undo
        = FileOps::undoRemoteMovesResult(moved.movesRemote, &error, FileOps::Progress());

    QVERIFY2(undo.failed.isEmpty(), qPrintable(error));
    QVERIFY2(undo.error.isEmpty(), qPrintable(undo.error));
    QCOMPARE(namesOf(sourceRoot), QStringList{QStringLiteral("tree")});
    QCOMPARE(namesOf(tree), (QStringList{QStringLiteral("empty"), QStringLiteral("inner.txt")}));
    QVERIFY(namesOf(targetRoot).isEmpty());
    QVERIFY(destination->dirs.isEmpty());
}

QTEST_MAIN(TestRemoteMove)
#include "test_remotemove.moc"
