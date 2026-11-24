#pragma once

#include <QHash>
#include <QKeySequence>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

// What a command acts on. Filled in by the caller so that a command never has
// to know whether it was triggered by a key, a button or (later) a script.
struct CommandContext
{
    QString sourcePath;
    QString destPath;
    QStringList selection;
};

struct Command
{
    QString id;
    QString title;
    QString category;
    std::function<void(CommandContext &)> run;
};

// The spine of the program. Every user-visible action is a command registered
// here; keys, buttons, context menus and scripts are only triggers that name a
// command id. Nothing in the UI is allowed to hold behaviour of its own.
class CommandRegistry
{
public:
    static CommandRegistry &instance();

    void add(const Command &command);
    void remove(const QString &id);
    void removePrefix(const QString &prefix);
    const Command *find(const QString &id) const;
    const QVector<Command> &all() const { return m_commands; }

    void bind(const QKeySequence &sequence, const QString &commandId);
    void unbind(const QString &commandId);
    void clearBindings() { m_keymap.clear(); }
    QString commandFor(const QKeySequence &sequence) const;
    QString shortcutFor(const QString &commandId) const;
    // Every sequence bound to one command, sorted: a command may have more than
    // one way in, and the shortcut sheet should not hide that.
    QStringList shortcutsFor(const QString &commandId) const;

    bool run(const QString &id, CommandContext &context);

private:
    CommandRegistry() = default;

    QVector<Command> m_commands;
    QHash<QString, int> m_index;
    QHash<QString, QString> m_keymap; // portable sequence text -> command id
};
