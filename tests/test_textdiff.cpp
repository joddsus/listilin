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

    void latin1IsNotSilentlyEqual()
    {
        // "café" and "cafè" in Latin-1 become the same replacement character
        // when both are forced through UTF-8. Decoding as Latin-1 must keep
        // them different.
        const QByteArray left = QByteArray("caf\xE9", 4);
        const QByteArray right = QByteArray("caf\xE8", 4);
        QVERIFY(!TextDiff::isValidUtf8(left));
        QVERIFY(!TextDiff::isValidUtf8(right));
        QVERIFY(TextDiff::isValidUtf8(QStringLiteral("café").toUtf8()));

        bool fellBack = false;
        const QString a = TextDiff::decodeText(left, true, &fellBack);
        QVERIFY(fellBack);
        const QString b = TextDiff::decodeText(right, true);
        QCOMPARE(a, QStringLiteral("café"));
        QCOMPARE(b, QStringLiteral("cafè"));

        const TextDiff::Diff diff = TextDiff::compare(a, b);
        QVERIFY(diff.ok);
        QVERIFY(diff.text != QStringLiteral("No differences.\n"));
        QVERIFY(diff.text.contains(QStringLiteral("+cafè")));
    }

    void refusesAComparisonThatWouldNeedTooMuchMemory()
    {
        QStringList many;
        for (int i = 0; i < 2000; ++i)
            many.append(QStringLiteral("line %1").arg(i));
        const TextDiff::Diff diff = TextDiff::compare(many, many);
        QVERIFY(!diff.ok);
        QVERIFY(!diff.error.isEmpty());
        QVERIFY(diff.text.isEmpty());
    }
};

QTEST_MAIN(TestTextDiff)
#include "test_textdiff.moc"