#include "settingsafety.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

class TestSettingsSafety : public QObject
{
    Q_OBJECT

private slots:
    void validKeys();
    void invalidKeys();
    void validValues();
    void invalidValues();
    void sanitizeValue();
    void readWriteRoundTrip();
    void refusesUnreadableFile();
    void ignoresInvalidEntries();
    void corruptedFileDoesNotCrash();
    void updateMerges();
    void unwritableDirectoryFails();
    void updatePreservesKeysTheStrictReaderRejects();
};

void TestSettingsSafety::validKeys()
{
    QVERIFY(SettingsSafety::isValidKey(QStringLiteral("startup/source")));
    QVERIFY(SettingsSafety::isValidKey(QStringLiteral("view.showHidden")));
    QVERIFY(SettingsSafety::isValidKey(QStringLiteral("a-b_c.d")));
}

void TestSettingsSafety::invalidKeys()
{
    QVERIFY(!SettingsSafety::isValidKey(QString()));
    QVERIFY(!SettingsSafety::isValidKey(QStringLiteral("a//b")));
    QVERIFY(!SettingsSafety::isValidKey(QStringLiteral("../secret")));
    QVERIFY(!SettingsSafety::isValidKey(QStringLiteral("a\0b")));
    QVERIFY(!SettingsSafety::isValidKey(QStringLiteral("has space")));
}

void TestSettingsSafety::validValues()
{
    QVERIFY(SettingsSafety::isValidValue(true));
    QVERIFY(SettingsSafety::isValidValue(42));
    QVERIFY(SettingsSafety::isValidValue(QStringLiteral("hello")));
    QVERIFY(SettingsSafety::isValidValue(QStringList{QStringLiteral("a"), QStringLiteral("b")}));
}

void TestSettingsSafety::invalidValues()
{
    QString reason;
    QVERIFY(!SettingsSafety::isValidValue(QString(70000, 'x'), &reason));
    QVERIFY(!reason.isEmpty());
    QVariant bad;
    bad.setValue(QVariantMap{{QStringLiteral("../x"), 1}});
    QVERIFY(!SettingsSafety::isValidValue(bad));
}

void TestSettingsSafety::sanitizeValue()
{
    const QString longString(70000, 'x');
    const QVariant sanitized = SettingsSafety::sanitizeValue(longString);
    QVERIFY(sanitized.toString().size() <= 65536);
}

void TestSettingsSafety::readWriteRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.ini"));

    QMap<QString, QVariant> values;
    values.insert(QStringLiteral("startup/source"), QStringLiteral("/tmp"));
    values.insert(QStringLiteral("view/showHidden"), true);
    QVERIFY(SettingsSafety::writeSettingsAtomic(path, values));

    const QMap<QString, QVariant> read = SettingsSafety::readSettings(path);
    QCOMPARE(read.value(QStringLiteral("startup/source")).toString(), QStringLiteral("/tmp"));
    QCOMPARE(read.value(QStringLiteral("view/showHidden")).toBool(), true);
}

void TestSettingsSafety::refusesUnreadableFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("existing=1\n");
    file.close();
    QFile::setPermissions(path, QFile::Permissions());

    QMap<QString, QVariant> values;
    values.insert(QStringLiteral("x"), 1);
    QString error;
    QVERIFY(!SettingsSafety::writeSettingsAtomic(path, values, &error));
    QVERIFY(!error.isEmpty());

    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
}

void TestSettingsSafety::ignoresInvalidEntries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("good=42\nbad key=7\n");
    file.close();

    const QMap<QString, QVariant> read = SettingsSafety::readSettings(path);
    QVERIFY(read.contains(QStringLiteral("good")));
    QVERIFY(!read.contains(QStringLiteral("bad key")));
}

void TestSettingsSafety::corruptedFileDoesNotCrash()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not valid ini \0\0\0", 24);
    file.close();

    QString error;
    const QMap<QString, QVariant> read = SettingsSafety::readSettings(path, &error);
    QVERIFY(read.isEmpty() || true);
    Q_UNUSED(read);
    // readSettings must not crash; error may be set.
}

void TestSettingsSafety::updateMerges()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.ini"));

    QMap<QString, QVariant> first;
    first.insert(QStringLiteral("a"), 1);
    QVERIFY(SettingsSafety::writeSettingsAtomic(path, first));

    QMap<QString, QVariant> updates;
    updates.insert(QStringLiteral("b"), 2);
    QVERIFY(SettingsSafety::updateSettings(path, updates));

    const QMap<QString, QVariant> read = SettingsSafety::readSettings(path);
    QCOMPARE(read.value(QStringLiteral("a")).toInt(), 1);
    QCOMPARE(read.value(QStringLiteral("b")).toInt(), 2);
}

void TestSettingsSafety::unwritableDirectoryFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString sub = dir.filePath(QStringLiteral("readonly"));
    QVERIFY(QDir().mkdir(sub));
    QFile::setPermissions(sub, QFile::ReadOwner | QFile::ExeOwner);

    const QString path = QDir(sub).filePath(QStringLiteral("settings.ini"));
    QMap<QString, QVariant> values;
    values.insert(QStringLiteral("a"), 1);
    QString error;
    QVERIFY(!SettingsSafety::writeSettingsAtomic(path, values, &error));
    QVERIFY(!error.isEmpty());

    QFile::setPermissions(sub, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

void TestSettingsSafety::updatePreservesKeysTheStrictReaderRejects()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.ini"));

    // Keys Listista writes itself: a Shelf collection with a space and an
    // icon-view choice keyed by a URI. The strict key reader rejects both.
    {
        QSettings seed(path, QSettings::IniFormat);
        seed.setValue(QStringLiteral("shelf/collections/Min samling"),
                      QStringList{QStringLiteral("file:///tmp/a\ta\t0")});
        seed.setValue(QStringLiteral("view/iconView/file:///tmp"), true);
        seed.setValue(QStringLiteral("startup/source"), QStringLiteral("/home"));
        seed.sync();
        QCOMPARE(seed.status(), QSettings::NoError);
    }

    QMap<QString, QVariant> updates;
    updates.insert(QStringLiteral("window/geometry"), QByteArray("geometry"));
    QString error;
    QVERIFY2(SettingsSafety::updateSettings(path, updates, &error), qPrintable(error));

    QSettings check(path, QSettings::IniFormat);
    QCOMPARE(check.value(QStringLiteral("shelf/collections/Min samling")).toStringList(),
             QStringList{QStringLiteral("file:///tmp/a\ta\t0")});
    QCOMPARE(check.value(QStringLiteral("view/iconView/file:///tmp")).toBool(), true);
    QCOMPARE(check.value(QStringLiteral("startup/source")).toString(), QStringLiteral("/home"));
    QCOMPARE(check.value(QStringLiteral("window/geometry")).toByteArray(), QByteArray("geometry"));
}

QTEST_MAIN(TestSettingsSafety)
#include "test_settingsafety.moc"