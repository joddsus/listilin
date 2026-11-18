#include <QtTest/QtTest>

#include "settingsmigration.h"

class TestSettingsMigration : public QObject
{
    Q_OBJECT
private slots:
    void copiesWhenCurrentMissing();
    void skipsWhenCurrentHasKeys();
    void skipsWhenLegacyEmpty();
    void preservesUnreadableCurrent();
};

void TestSettingsMigration::copiesWhenCurrentMissing()
{
    SettingsMigration::State state;
    state.legacyHasKeys = true;
    state.currentFileExists = false;
    state.currentFileReadable = true;
    state.currentHasKeys = false;
    QVERIFY(SettingsMigration::shouldCopyLegacy(state));
}

void TestSettingsMigration::skipsWhenCurrentHasKeys()
{
    SettingsMigration::State state;
    state.legacyHasKeys = true;
    state.currentFileExists = true;
    state.currentFileReadable = true;
    state.currentHasKeys = true;
    QVERIFY(!SettingsMigration::shouldCopyLegacy(state));
}

void TestSettingsMigration::skipsWhenLegacyEmpty()
{
    SettingsMigration::State state;
    state.legacyHasKeys = false;
    state.currentFileExists = false;
    state.currentFileReadable = true;
    state.currentHasKeys = false;
    QVERIFY(!SettingsMigration::shouldCopyLegacy(state));
}

void TestSettingsMigration::preservesUnreadableCurrent()
{
    SettingsMigration::State state;
    state.legacyHasKeys = true;
    state.currentFileExists = true;
    state.currentFileReadable = false;
    state.currentHasKeys = false;
    QVERIFY(!SettingsMigration::shouldCopyLegacy(state));
}

QTEST_MAIN(TestSettingsMigration)
#include "test_settingsmigration.moc"