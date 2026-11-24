#include "textdiff.h"

#include <QtTest>

class TestTextDiff : public QObject
{
    Q_OBJECT

private slots:
    void identical()
    {
        const QString diff = TextDiff::unifiedDiff(QStringLiteral("a\nb\nc"), QStringLiteral("a\nb\nc"));
        QCOMPARE(diff, QStringLiteral("No differences.\n"));
    }

    void simpleChange()
    {
        const QString diff = TextDiff::unifiedDiff(QStringLiteral("a\nb\nc"), QStringLiteral("a\nX\nc"));
        QVERIFY(diff.contains(QLatin1String("-b")));
        QVERIFY(diff.contains(QLatin1String("+X")));
        QVERIFY(diff.contains(QLatin1String("@@")));
    }

    void addition()
    {
        const QString diff = TextDiff::unifiedDiff(QStringLiteral("a\nc"), QStringLiteral("a\nb\nc"));
        QVERIFY(diff.contains(QLatin1String("+b")));
    }
};

QTEST_MAIN(TestTextDiff)
#include "test_textdiff.moc"