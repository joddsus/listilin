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

// A remote that accepts uploads and lists them, but refuses every delete. It
// proves that a move out of a server keeps both copies and reports the refusal
// instead of pretending the source is gone.
class NoRemoveBackend : public VfsBackend
{
public:
    QString scheme() const override { return QStringLiteral("fake"); }
    bool writable() const override { return true; }

    bool list(const VfsUri &uri, bool, QVector<Entry> *entries, QString *) override
    {
        entries->clear();
        const QString prefix = uri.toString();
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            if (!it.key().startsWith(prefix))
                continue;
            QString rest = it.key().mid(prefix.size());
            if (rest.startsWith(QLatin1Char('/')))
                rest.remove(0, 1);
            if (rest.isEmpty() || rest.contains(QLatin1Char('/')))
                continue;
            Entry entry;
            entry.name = rest;
            entry.size = it.value().size();
            entry.hasSize = true;
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
        files.insert(VfsUri::childUri(targetDir.toString(), name), in.readAll());
        return true;
    }

    bool mkdir(const VfsUri &targetDir, const QString &name, QString *) override
    {
        dirs.insert(VfsUri::childUri(targetDir.toString(), name));
        return true;
    }

    bool remove(const VfsUri &, bool, QString *error) override
    {
        if (error)
            *error = QStringLiteral("This server does not allow deletes.");
        return false;
    }

    QHash<QString, QByteArray> files;
    QSet<QString> dirs;
};

// A server whose listing is honest about the size but whose download stops
// halfway. A move must notice before deleting the original.
class TruncatingBackend : public VfsBackend
{
public:
    QString scheme() const override { return QStringLiteral("trunc"); }
    bool writable() const override { return true; }

    bool list(const VfsUri &uri, bool, QVector<Entry> *entries, QString *) override
    {
        entries->clear();
        const QString prefix = uri.toString();
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            if (!it.key().startsWith(prefix))
                continue;
            QString rest = it.key().mid(prefix.size());
            if (rest.startsWith(QLatin1Char('/')))
                rest.remove(0, 1);
            if (rest.isEmpty() || rest.contains(QLatin1Char('/')))
                continue;
            Entry entry;
            entry.name = rest;
            entry.size = it.value().size();
            entry.hasSize = true;
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
        out.write(it.value().left(it.value().size() / 2));
        return true;
    }

    bool put(const VfsUri &targetDir, const QString &name, const QString &localFile,
             QString *) override
    {
        QFile in(localFile);
        if (!in.open(QIODevice::ReadOnly))
            return false;
        files.insert(VfsUri::childUri(targetDir.toString(), name), in.readAll());
        return true;
    }

    bool remove(const VfsUri &uri, bool, QString *) override
    {
        return files.remove(uri.toString()) > 0;
    }

    QHash<QString, QByteArray> files;
};

// A backend that lists a checksum which does not match the bytes it serves.
// The size is right, so only the digest can catch the bad copy.
class WrongDigestBackend : public VfsBackend
{
public:
    QString scheme() const override { return QStringLiteral("baddigest"); }
    bool writable() const override { return true; }

    bool list(const VfsUri &uri, bool, QVector<Entry> *entries, QString *) override
    {
        entries->clear();
        const QString prefix = uri.toString();
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            if (!it.key().startsWith(prefix))
                continue;
            QString rest = it.key().mid(prefix.size());
            if (rest.startsWith(QLatin1Char('/')))
                rest.remove(0, 1);
            if (rest.isEmpty() || rest.contains(QLatin1Char('/')))
                continue;
            Entry entry;
            entry.name = rest;
            entry.size = it.value().size();
            entry.hasSize = true;
            entry.digest = QString(64, QLatin1Char('0'));
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
        files.insert(VfsUri::childUri(targetDir.toString(), name), in.readAll());
        return true;
    }

    bool remove(const VfsUri &uri, bool, QString *) override
    {
        return files.remove(uri.toString()) > 0;
    }

    QHash<QString, QByteArray> files;
};

class TestMoveOut : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void movesAMemberOutOfAnArchive();
    void movesAFolderOutIncludingAnEmptyFolder();
    void keepsBothCopiesWhenTheServerRefusesTheDelete();
    void rejectsATruncatedDownloadAndKeepsBoth();
    void rejectsAMismatchedDigestAndKeepsBoth();
    void undoPutsTheMemberBackAndRemovesTheCopy();
    void undoRefusesAChangedCopy();
    void undoRefusesWhenTheOriginalNameIsTaken();
    void undoPutsAFolderBack();
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

static QString archiveUri(const QString &path)
{
    return VfsUri::archiveUriFor(QStringLiteral("file://") + path);
}

static QStringList memberNames(const QString &uri)
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

void TestMoveOut::initTestCase()
{
    registerBackends();
}

void TestMoveOut::movesAMemberOutOfAnArchive()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/member.txt");
    writeFile(source, "member contents");
    const QString archive = archiveUri(tmp.path() + QStringLiteral("/source.zip"));
    QString error;
    QVERIFY2(Vfs::instance().put(archive, QStringLiteral("member.txt"), source, &error),
             qPrintable(error));

    const QString targetDir = tmp.path() + QStringLiteral("/out");
    QVERIFY(QDir().mkpath(targetDir));

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(archive, QStringLiteral("member.txt"));
    const FileOps::Result result
        = FileOps::copyUrisResult({item}, targetDir, true, &error, FileOps::Progress());

    QVERIFY2(result.failed.isEmpty(), qPrintable(error));
    QCOMPARE(readFile(targetDir + QStringLiteral("/member.txt")),
             QByteArray("member contents"));
    QVERIFY(memberNames(archive).isEmpty());
    QCOMPARE(result.movesOut.size(), 1);
    QCOMPARE(result.movesOut.first().sourceName, QStringLiteral("member.txt"));
    QCOMPARE(result.movesOut.first().localPath, targetDir + QStringLiteral("/member.txt"));
    QVERIFY(!result.movesOut.first().fingerprint.isEmpty());
}

void TestMoveOut::movesAFolderOutIncludingAnEmptyFolder()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archive = archiveUri(tmp.path() + QStringLiteral("/tree.zip"));
    const QString payload = tmp.path() + QStringLiteral("/inner.txt");
    writeFile(payload, "inner");
    QString error;
    QVERIFY(Vfs::instance().mkdir(archive, QStringLiteral("tree"), &error));
    const QString treeUri = VfsUri::childUri(archive, QStringLiteral("tree"));
    QVERIFY(Vfs::instance().mkdir(treeUri, QStringLiteral("empty"), &error));
    QVERIFY2(Vfs::instance().put(treeUri, QStringLiteral("inner.txt"), payload, &error),
             qPrintable(error));

    const QString targetDir = tmp.path() + QStringLiteral("/out");
    QVERIFY(QDir().mkpath(targetDir));

    FileOps::CopyItem item;
    item.uri = treeUri;
    item.isDir = true;
    const FileOps::Result result
        = FileOps::copyUrisResult({item}, targetDir, true, &error, FileOps::Progress());

    QVERIFY2(result.failed.isEmpty(), qPrintable(error));
    QCOMPARE(readFile(targetDir + QStringLiteral("/tree/inner.txt")), QByteArray("inner"));
    QVERIFY(QFileInfo(targetDir + QStringLiteral("/tree/empty")).isDir());
    QVERIFY(memberNames(archive).isEmpty());
    QCOMPARE(result.movesOut.size(), 1);
    QVERIFY(result.movesOut.first().isDir);
}

void TestMoveOut::keepsBothCopiesWhenTheServerRefusesTheDelete()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    auto backend = std::make_shared<NoRemoveBackend>();
    Vfs::instance().addBackend(backend);

    const QString source = tmp.path() + QStringLiteral("/remote.txt");
    writeFile(source, "remote contents");
    const QString remoteRoot = QStringLiteral("fake://host/folder");
    QString error;
    QVERIFY(Vfs::instance().put(remoteRoot, QStringLiteral("remote.txt"), source, &error));

    const QString targetDir = tmp.path() + QStringLiteral("/out");
    QVERIFY(QDir().mkpath(targetDir));

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(remoteRoot, QStringLiteral("remote.txt"));
    const FileOps::Result result
        = FileOps::copyUrisResult({item}, targetDir, true, &error, FileOps::Progress());

    QCOMPARE(result.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("could not be removed")), qPrintable(error));
    QCOMPARE(readFile(targetDir + QStringLiteral("/remote.txt")),
             QByteArray("remote contents"));
    QVERIFY(result.movesOut.isEmpty());
    QVERIFY(backend->files.contains(item.uri));
}

void TestMoveOut::rejectsATruncatedDownloadAndKeepsBoth()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    auto backend = std::make_shared<TruncatingBackend>();
    Vfs::instance().addBackend(backend);

    const QString source = tmp.path() + QStringLiteral("/big.bin");
    writeFile(source, QByteArray(4000, 'x'));
    const QString remoteRoot = QStringLiteral("trunc://host/folder");
    QString error;
    QVERIFY(Vfs::instance().put(remoteRoot, QStringLiteral("big.bin"), source, &error));

    const QString targetDir = tmp.path() + QStringLiteral("/out");
    QVERIFY(QDir().mkpath(targetDir));

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(remoteRoot, QStringLiteral("big.bin"));
    const FileOps::Result result
        = FileOps::copyUrisResult({item}, targetDir, true, &error, FileOps::Progress());

    QCOMPARE(result.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("did not arrive intact")), qPrintable(error));
    QVERIFY2(error.contains(QStringLiteral("Both copies were kept")), qPrintable(error));
    QVERIFY(result.movesOut.isEmpty());
    // The original is untouched even though this backend would have deleted it.
    QCOMPARE(backend->files.value(item.uri).size(), 4000);
    // The truncated download is still there for inspection, not silently lost.
    QCOMPARE(QFileInfo(targetDir + QStringLiteral("/big.bin")).size(), qint64(2000));
}

void TestMoveOut::rejectsAMismatchedDigestAndKeepsBoth()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    auto backend = std::make_shared<WrongDigestBackend>();
    Vfs::instance().addBackend(backend);

    const QString source = tmp.path() + QStringLiteral("/member.txt");
    writeFile(source, "member contents");
    const QString remoteRoot = QStringLiteral("baddigest://host/folder");
    QString error;
    QVERIFY(Vfs::instance().put(remoteRoot, QStringLiteral("member.txt"), source, &error));

    const QString targetDir = tmp.path() + QStringLiteral("/out");
    QVERIFY(QDir().mkpath(targetDir));

    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(remoteRoot, QStringLiteral("member.txt"));
    const FileOps::Result result
        = FileOps::copyUrisResult({item}, targetDir, true, &error, FileOps::Progress());

    QCOMPARE(result.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("checksum does not match")), qPrintable(error));
    QVERIFY(result.movesOut.isEmpty());
    QVERIFY(backend->files.contains(item.uri));
    QCOMPARE(readFile(targetDir + QStringLiteral("/member.txt")),
             QByteArray("member contents"));
}

void TestMoveOut::undoPutsTheMemberBackAndRemovesTheCopy()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/member.txt");
    writeFile(source, "member contents");
    const QString archive = archiveUri(tmp.path() + QStringLiteral("/undo.zip"));
    QString error;
    QVERIFY(Vfs::instance().put(archive, QStringLiteral("member.txt"), source, &error));

    const QString targetDir = tmp.path() + QStringLiteral("/out");
    QVERIFY(QDir().mkpath(targetDir));
    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(archive, QStringLiteral("member.txt"));
    const FileOps::Result moved
        = FileOps::copyUrisResult({item}, targetDir, true, &error, FileOps::Progress());
    QVERIFY2(moved.failed.isEmpty(), qPrintable(error));
    QCOMPARE(moved.movesOut.size(), 1);

    const FileOps::Result undo
        = FileOps::undoMovesOutResult(moved.movesOut, &error, FileOps::Progress());
    QVERIFY2(undo.failed.isEmpty(), qPrintable(error));
    QVERIFY2(undo.error.isEmpty(), qPrintable(undo.error));
    QVERIFY(!QFile::exists(targetDir + QStringLiteral("/member.txt")));
    QVERIFY(memberNames(archive).contains(QStringLiteral("member.txt")));

    const QString fetched = tmp.path() + QStringLiteral("/fetched.txt");
    QVERIFY(Vfs::instance().fetch(VfsUri::childUri(archive, QStringLiteral("member.txt")),
                                  fetched, &error));
    QCOMPARE(readFile(fetched), QByteArray("member contents"));
}

void TestMoveOut::undoRefusesAChangedCopy()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/member.txt");
    writeFile(source, "original");
    const QString archive = archiveUri(tmp.path() + QStringLiteral("/changed.zip"));
    QString error;
    QVERIFY(Vfs::instance().put(archive, QStringLiteral("member.txt"), source, &error));

    const QString targetDir = tmp.path() + QStringLiteral("/out");
    QVERIFY(QDir().mkpath(targetDir));
    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(archive, QStringLiteral("member.txt"));
    const FileOps::Result moved
        = FileOps::copyUrisResult({item}, targetDir, true, &error, FileOps::Progress());
    QVERIFY2(moved.failed.isEmpty(), qPrintable(error));

    writeFile(targetDir + QStringLiteral("/member.txt"), "edited after the move");

    const FileOps::Result undo
        = FileOps::undoMovesOutResult(moved.movesOut, &error, FileOps::Progress());
    QCOMPARE(undo.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("changed")), qPrintable(error));
    QCOMPARE(readFile(targetDir + QStringLiteral("/member.txt")),
             QByteArray("edited after the move"));
    QVERIFY(memberNames(archive).isEmpty());
}

void TestMoveOut::undoRefusesWhenTheOriginalNameIsTaken()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.path() + QStringLiteral("/member.txt");
    writeFile(source, "original");
    const QString archive = archiveUri(tmp.path() + QStringLiteral("/taken.zip"));
    QString error;
    QVERIFY(Vfs::instance().put(archive, QStringLiteral("member.txt"), source, &error));

    const QString targetDir = tmp.path() + QStringLiteral("/out");
    QVERIFY(QDir().mkpath(targetDir));
    FileOps::CopyItem item;
    item.uri = VfsUri::childUri(archive, QStringLiteral("member.txt"));
    const FileOps::Result moved
        = FileOps::copyUrisResult({item}, targetDir, true, &error, FileOps::Progress());
    QVERIFY2(moved.failed.isEmpty(), qPrintable(error));

    const QString other = tmp.path() + QStringLiteral("/other.txt");
    writeFile(other, "somebody else's file");
    QVERIFY(Vfs::instance().put(archive, QStringLiteral("member.txt"), other, &error));

    const FileOps::Result undo
        = FileOps::undoMovesOutResult(moved.movesOut, &error, FileOps::Progress());
    QCOMPARE(undo.failed.size(), 1);
    QVERIFY2(error.contains(QStringLiteral("already exists")), qPrintable(error));
    QVERIFY(QFile::exists(targetDir + QStringLiteral("/member.txt")));
}

void TestMoveOut::undoPutsAFolderBack()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString archive = archiveUri(tmp.path() + QStringLiteral("/folder.zip"));
    const QString payload = tmp.path() + QStringLiteral("/inner.txt");
    writeFile(payload, "inner");
    QString error;
    QVERIFY(Vfs::instance().mkdir(archive, QStringLiteral("tree"), &error));
    const QString treeUri = VfsUri::childUri(archive, QStringLiteral("tree"));
    QVERIFY(Vfs::instance().mkdir(treeUri, QStringLiteral("empty"), &error));
    QVERIFY2(Vfs::instance().put(treeUri, QStringLiteral("inner.txt"), payload, &error),
             qPrintable(error));

    const QString targetDir = tmp.path() + QStringLiteral("/out");
    QVERIFY(QDir().mkpath(targetDir));
    FileOps::CopyItem item;
    item.uri = treeUri;
    item.isDir = true;
    const FileOps::Result moved
        = FileOps::copyUrisResult({item}, targetDir, true, &error, FileOps::Progress());
    QVERIFY2(moved.failed.isEmpty(), qPrintable(error));
    QCOMPARE(moved.movesOut.size(), 1);

    const FileOps::Result undo
        = FileOps::undoMovesOutResult(moved.movesOut, &error, FileOps::Progress());
    QVERIFY2(undo.failed.isEmpty(), qPrintable(error));
    QVERIFY2(undo.error.isEmpty(), qPrintable(undo.error));
    QVERIFY(!QFileInfo::exists(targetDir + QStringLiteral("/tree")));

    const QString fetched = tmp.path() + QStringLiteral("/fetched.txt");
    QVERIFY(Vfs::instance().fetch(
        VfsUri::childUri(treeUri, QStringLiteral("inner.txt")), fetched, &error));
    QCOMPARE(readFile(fetched), QByteArray("inner"));
    QVERIFY(memberNames(treeUri).contains(QStringLiteral("empty")));
}

QTEST_MAIN(TestMoveOut)
#include "test_moveout.moc"
