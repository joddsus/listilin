#include <QtTest/QtTest>

#include "renamebatch.h"

class TestRenameBatch : public QObject
{
    Q_OBJECT
private slots:
    void findReplaceKeepsExtension();
    void prefixAndSuffix();
    void numberingFromStartValue();
    void dropExtension();
    void multiDotName();
    void dotfileHasNoExtension();
    void emptyResultKeepsOriginalName();
    void duplicateNamesFlagged();
    void unchangedNamesCounted();
    void transformationOrder();
};

void TestRenameBatch::findReplaceKeepsExtension()
{
    RenameBatch::Options options;
    options.find = QStringLiteral("draft");
    options.replace = QStringLiteral("final");

    const RenameBatch::Plan plan
        = RenameBatch::plan({QStringLiteral("draft.txt")}, options);
    QCOMPARE(plan.newNames, QStringList{QStringLiteral("final.txt")});
    QVERIFY(!plan.unchanged.first());
    QCOMPARE(plan.renameCount(), 1);
}

void TestRenameBatch::prefixAndSuffix()
{
    RenameBatch::Options options;
    options.prefix = QStringLiteral("2026-");
    options.suffix = QStringLiteral("-rev");

    const RenameBatch::Plan plan
        = RenameBatch::plan({QStringLiteral("report.txt")}, options);
    QCOMPARE(plan.newNames, QStringList{QStringLiteral("2026-report-rev.txt")});
}

void TestRenameBatch::numberingFromStartValue()
{
    RenameBatch::Options options;
    options.numbering = true;
    options.startNumber = 3;

    const RenameBatch::Plan plan = RenameBatch::plan(
        {QStringLiteral("a.txt"), QStringLiteral("b.txt")}, options);
    QCOMPARE(plan.newNames, (QStringList{QStringLiteral("a3.txt"),
                                         QStringLiteral("b4.txt")}));
}

void TestRenameBatch::dropExtension()
{
    RenameBatch::Options options;
    options.keepExtension = false;

    const RenameBatch::Plan plan
        = RenameBatch::plan({QStringLiteral("report.tar.gz")}, options);
    QCOMPARE(plan.newNames, QStringList{QStringLiteral("report.tar")});
}

void TestRenameBatch::multiDotName()
{
    RenameBatch::Options options;
    options.find = QStringLiteral(".");
    options.replace = QStringLiteral("_");

    const RenameBatch::Plan plan
        = RenameBatch::plan({QStringLiteral("photo.2026.jpg")}, options);
    QCOMPARE(plan.newNames, QStringList{QStringLiteral("photo_2026.jpg")});
}

void TestRenameBatch::dotfileHasNoExtension()
{
    RenameBatch::Options untouched;
    const RenameBatch::Plan kept
        = RenameBatch::plan({QStringLiteral(".bashrc")}, untouched);
    QCOMPARE(kept.newNames, QStringList{QStringLiteral(".bashrc")});
    QVERIFY(kept.unchanged.first());

    RenameBatch::Options prefixed;
    prefixed.prefix = QStringLiteral("x-");
    const RenameBatch::Plan renamed
        = RenameBatch::plan({QStringLiteral(".bashrc")}, prefixed);
    QCOMPARE(renamed.newNames, QStringList{QStringLiteral("x-.bashrc")});

    const RenameBatch::Plan trailing
        = RenameBatch::plan({QStringLiteral("name.")}, untouched);
    QCOMPARE(trailing.newNames, QStringList{QStringLiteral("name.")});
    QVERIFY(trailing.unchanged.first());

    const RenameBatch::Plan hidden
        = RenameBatch::plan({QStringLiteral("..foo")}, untouched);
    QCOMPARE(hidden.newNames, QStringList{QStringLiteral("..foo")});
    QVERIFY(hidden.unchanged.first());
}

void TestRenameBatch::emptyResultKeepsOriginalName()
{
    RenameBatch::Options options;
    options.find = QStringLiteral("a");
    options.replace = QString();
    options.keepExtension = false;

    const RenameBatch::Plan plan
        = RenameBatch::plan({QStringLiteral("a.txt")}, options);
    QCOMPARE(plan.newNames, QStringList{QStringLiteral("a.txt")});
    QVERIFY(plan.unchanged.first());
    QCOMPARE(plan.renameCount(), 0);
}

void TestRenameBatch::duplicateNamesFlagged()
{
    RenameBatch::Options options;
    options.find = QStringLiteral("one");
    options.replace = QStringLiteral("two");

    const RenameBatch::Plan plan = RenameBatch::plan(
        {QStringLiteral("one.txt"), QStringLiteral("two.txt")}, options);
    QCOMPARE(plan.newNames, (QStringList{QStringLiteral("two.txt"),
                                         QStringLiteral("two.txt")}));
    QCOMPARE(plan.duplicate, (QVector<bool>{false, true}));
}

void TestRenameBatch::unchangedNamesCounted()
{
    RenameBatch::Options options;
    options.find = QStringLiteral("old");
    options.replace = QStringLiteral("new");

    const RenameBatch::Plan plan = RenameBatch::plan(
        {QStringLiteral("keep.txt"), QStringLiteral("old.txt")}, options);
    QCOMPARE(plan.unchanged, (QVector<bool>{true, false}));
    QCOMPARE(plan.renameCount(), 1);
    QCOMPARE(plan.currentNames, (QStringList{QStringLiteral("keep.txt"),
                                             QStringLiteral("old.txt")}));
}

void TestRenameBatch::transformationOrder()
{
    RenameBatch::Options options;
    options.find = QStringLiteral("a");
    options.replace = QStringLiteral("b");
    options.prefix = QStringLiteral("p-");
    options.suffix = QStringLiteral("-s");
    options.numbering = true;
    options.startNumber = 2;

    const RenameBatch::Plan plan
        = RenameBatch::plan({QStringLiteral("a.txt")}, options);
    QCOMPARE(plan.newNames, QStringList{QStringLiteral("p-b-s2.txt")});
}

QTEST_MAIN(TestRenameBatch)
#include "test_renamebatch.moc"
