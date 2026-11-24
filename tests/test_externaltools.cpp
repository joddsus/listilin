#include "externaltools.h"

#include <QtTest>

class TestExternalTools : public QObject
{
    Q_OBJECT

private slots:
    void parseValid()
    {
        ExternalTools::Tool tool;
        QVERIFY(ExternalTools::parse(QStringLiteral("Editor|/usr/bin/vim|%d/file.txt"), &tool));
        QCOMPARE(tool.name, QStringLiteral("Editor"));
        QCOMPARE(tool.program, QStringLiteral("/usr/bin/vim"));
        QCOMPARE(tool.arguments, QStringList{QStringLiteral("%d/file.txt")});
    }

    void parseMissingArgs()
    {
        ExternalTools::Tool tool;
        QVERIFY(ExternalTools::parse(QStringLiteral("Term|xterm"), &tool));
        QCOMPARE(tool.name, QStringLiteral("Term"));
        QCOMPARE(tool.program, QStringLiteral("xterm"));
        QVERIFY(tool.arguments.isEmpty());
    }

    void parseMalformed()
    {
        ExternalTools::Tool tool;
        QVERIFY(!ExternalTools::parse(QStringLiteral("onlyname"), &tool));
        QVERIFY(!ExternalTools::parse(QStringLiteral("|prog"), &tool));
    }

    void roundTrip()
    {
        ExternalTools::Tool tool{QStringLiteral("T"), QStringLiteral("p"), {QStringLiteral("a"), QStringLiteral("b")}};
        const QString entry = ExternalTools::serialise(tool);
        ExternalTools::Tool parsed;
        QVERIFY(ExternalTools::parse(entry, &parsed));
        QCOMPARE(parsed.name, tool.name);
        QCOMPARE(parsed.program, tool.program);
        QCOMPARE(parsed.arguments, tool.arguments);
    }

    void parseAllSkipsBad()
    {
        const QStringList entries = {QStringLiteral("A|a"), QStringLiteral("bad"), QStringLiteral("B|b|x y")};
        const auto tools = ExternalTools::parseAll(entries);
        QCOMPARE(tools.size(), 2);
        const QStringList expected = {QStringLiteral("x"), QStringLiteral("y")};
        QCOMPARE(tools.at(1).arguments, expected);
    }
};

QTEST_MAIN(TestExternalTools)
#include "test_externaltools.moc"