#include "command.h"

#include <QtTest>
#include <QKeySequence>

class TestCommandRegistry : public QObject
{
    Q_OBJECT

private slots:
    void removeDropsKeymapEntries()
    {
        CommandRegistry &registry = CommandRegistry::instance();
        registry.add({QStringLiteral("cmd.remove.test"), QStringLiteral("Remove Test"), QStringLiteral("Test"),
                      [](CommandContext &) {}});

        const QKeySequence seq1(Qt::CTRL | Qt::Key_A);
        const QKeySequence seq2(Qt::CTRL | Qt::Key_B);
        const QKeySequence seqOther(Qt::CTRL | Qt::Key_C);

        registry.bind(seq1, QStringLiteral("cmd.remove.test"));
        registry.bind(seq2, QStringLiteral("cmd.remove.test"));
        registry.add({QStringLiteral("cmd.keep.test"), QStringLiteral("Keep Test"), QStringLiteral("Test"),
                      [](CommandContext &) {}});
        registry.bind(seqOther, QStringLiteral("cmd.keep.test"));

        QCOMPARE(registry.shortcutsFor(QStringLiteral("cmd.remove.test")).size(), 2);

        registry.remove(QStringLiteral("cmd.remove.test"));

        QVERIFY(registry.shortcutsFor(QStringLiteral("cmd.remove.test")).isEmpty());
        QVERIFY(registry.commandFor(seq1).isEmpty());
        QVERIFY(registry.commandFor(seq2).isEmpty());
        // Unrelated binding survives.
        QCOMPARE(registry.commandFor(seqOther), QStringLiteral("cmd.keep.test"));

        registry.remove(QStringLiteral("cmd.keep.test"));
    }

    void removePrefixDropsKeymapEntries()
    {
        CommandRegistry &registry = CommandRegistry::instance();

        registry.add({QStringLiteral("tools.external.0"), QStringLiteral("Tool 0"), QStringLiteral("Tools"),
                      [](CommandContext &) {}});
        registry.add({QStringLiteral("tools.external.1"), QStringLiteral("Tool 1"), QStringLiteral("Tools"),
                      [](CommandContext &) {}});
        registry.add({QStringLiteral("tools.keep"), QStringLiteral("Keep"), QStringLiteral("Tools"),
                      [](CommandContext &) {}});

        const QKeySequence seq0(Qt::CTRL | Qt::Key_1);
        const QKeySequence seq1(Qt::CTRL | Qt::Key_2);
        const QKeySequence seqKeep(Qt::CTRL | Qt::Key_3);
        const QKeySequence seqMultiA(Qt::CTRL | Qt::Key_4);
        const QKeySequence seqMultiB(Qt::CTRL | Qt::Key_5);

        registry.bind(seq0, QStringLiteral("tools.external.0"));
        registry.bind(seq1, QStringLiteral("tools.external.1"));
        registry.bind(seqKeep, QStringLiteral("tools.keep"));
        // Multiple bindings for one removed command.
        registry.bind(seqMultiA, QStringLiteral("tools.external.1"));
        registry.bind(seqMultiB, QStringLiteral("tools.external.1"));

        registry.removePrefix(QStringLiteral("tools.external."));

        QVERIFY(registry.find(QStringLiteral("tools.external.0")) == nullptr);
        QVERIFY(registry.find(QStringLiteral("tools.external.1")) == nullptr);
        QVERIFY(registry.shortcutsFor(QStringLiteral("tools.external.0")).isEmpty());
        QVERIFY(registry.shortcutsFor(QStringLiteral("tools.external.1")).isEmpty());
        QVERIFY(registry.commandFor(seq0).isEmpty());
        QVERIFY(registry.commandFor(seq1).isEmpty());
        QVERIFY(registry.commandFor(seqMultiA).isEmpty());
        QVERIFY(registry.commandFor(seqMultiB).isEmpty());
        // Unrelated command and binding survive.
        QVERIFY(registry.find(QStringLiteral("tools.keep")) != nullptr);
        QCOMPARE(registry.commandFor(seqKeep), QStringLiteral("tools.keep"));

        registry.remove(QStringLiteral("tools.keep"));
    }
};

QTEST_MAIN(TestCommandRegistry)
#include "test_commandregistry.moc"