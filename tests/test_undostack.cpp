#include "undostack.h"

#include <QtTest>

class TestUndoStack : public QObject
{
    Q_OBJECT

private slots:
    void emptyInitially()
    {
        UndoStack::Stack stack;
        QVERIFY(!stack.canUndo());
        QCOMPARE(stack.size(), 0);
    }

    void pushAndPop()
    {
        UndoStack::Stack stack;
        UndoStack::Entry e;
        e.type = UndoStack::OpType::Rename;
        e.sources = {QStringLiteral("/a/old")};
        e.destinations = {QStringLiteral("/a/new")};
        stack.push(e);
        QVERIFY(stack.canUndo());
        QCOMPARE(stack.size(), 1);

        UndoStack::Entry popped = stack.pop();
        QCOMPARE(popped.sources.first(), QStringLiteral("/a/old"));
        QCOMPARE(popped.destinations.first(), QStringLiteral("/a/new"));
        QVERIFY(!stack.canUndo());
    }

    void nonUndoableIgnored()
    {
        UndoStack::Stack stack;
        UndoStack::Entry e;
        e.undoable = false;
        stack.push(e);
        QVERIFY(!stack.canUndo());
    }

    void maxSizeEvictsOldest()
    {
        UndoStack::Stack stack(2);
        for (int i = 0; i < 3; ++i) {
            UndoStack::Entry e;
            e.sources = {QStringLiteral("/x%1").arg(i)};
            stack.push(e);
        }
        QCOMPARE(stack.size(), 2);
        UndoStack::Entry first = stack.pop();
        // last pushed was i=2, so first pop is i=2; next is i=1
        QCOMPARE(first.sources.first(), QStringLiteral("/x2"));
    }

    void clear()
    {
        UndoStack::Stack stack;
        UndoStack::Entry e;
        stack.push(e);
        stack.clear();
        QVERIFY(!stack.canUndo());
    }

    void peekDoesNotRemove()
    {
        UndoStack::Stack stack;
        UndoStack::Entry e;
        e.sources = {QStringLiteral("/p")};
        stack.push(e);
        UndoStack::Entry p;
        QVERIFY(stack.peek(&p));
        QCOMPARE(p.sources.first(), QStringLiteral("/p"));
        QVERIFY(stack.canUndo());
    }
};

QTEST_MAIN(TestUndoStack)
#include "test_undostack.moc"