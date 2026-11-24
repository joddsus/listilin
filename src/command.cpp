#include "command.h"

CommandRegistry &CommandRegistry::instance()
{
    static CommandRegistry registry;
    return registry;
}

void CommandRegistry::add(const Command &command)
{
    if (command.id.isEmpty())
        return;

    const auto it = m_index.constFind(command.id);
    if (it != m_index.constEnd()) {
        m_commands[*it] = command;
        return;
    }

    m_index.insert(command.id, m_commands.size());
    m_commands.append(command);
}

void CommandRegistry::remove(const QString &id)
{
    const auto it = m_index.constFind(id);
    if (it == m_index.constEnd())
        return;
    const int index = *it;
    m_commands.removeAt(index);
    m_index.clear();
    for (int i = 0; i < m_commands.size(); ++i)
        m_index.insert(m_commands.at(i).id, i);

    // Drop any keymap entries that pointed at the removed command, so
    // runtime edits (e.g. external tools) don't leave dead shortcuts.
    for (auto kit = m_keymap.begin(); kit != m_keymap.end();) {
        if (kit.value() == id)
            kit = m_keymap.erase(kit);
        else
            ++kit;
    }
}

void CommandRegistry::removePrefix(const QString &prefix)
{
    for (int i = m_commands.size() - 1; i >= 0; --i) {
        if (m_commands.at(i).id.startsWith(prefix))
            m_commands.removeAt(i);
    }
    m_index.clear();
    for (int i = 0; i < m_commands.size(); ++i)
        m_index.insert(m_commands.at(i).id, i);

    // Drop any keymap entries that pointed at removed commands.
    for (auto kit = m_keymap.begin(); kit != m_keymap.end();) {
        if (kit.value().startsWith(prefix))
            kit = m_keymap.erase(kit);
        else
            ++kit;
    }
}

const Command *CommandRegistry::find(const QString &id) const
{
    const auto it = m_index.constFind(id);
    if (it == m_index.constEnd())
        return nullptr;
    return &m_commands[*it];
}

void CommandRegistry::bind(const QKeySequence &sequence, const QString &commandId)
{
    if (sequence.isEmpty())
        return;

    m_keymap.insert(sequence.toString(QKeySequence::PortableText), commandId);
}

void CommandRegistry::unbind(const QString &commandId)
{
    for (auto it = m_keymap.begin(); it != m_keymap.end();) {
        if (it.value() == commandId)
            it = m_keymap.erase(it);
        else
            ++it;
    }
}

QString CommandRegistry::commandFor(const QKeySequence &sequence) const
{
    if (sequence.isEmpty())
        return {};
    return m_keymap.value(sequence.toString(QKeySequence::PortableText));
}

QStringList CommandRegistry::shortcutsFor(const QString &commandId) const
{
    QStringList sequences;
    for (auto it = m_keymap.constBegin(); it != m_keymap.constEnd(); ++it) {
        if (it.value() == commandId)
            sequences.append(it.key());
    }
    sequences.sort();
    return sequences;
}

QString CommandRegistry::shortcutFor(const QString &commandId) const
{
    const QStringList sequences = shortcutsFor(commandId);
    return sequences.isEmpty() ? QString() : sequences.first();
}

bool CommandRegistry::run(const QString &id, CommandContext &context)
{
    const Command *command = find(id);
    if (!command || !command->run)
        return false;

    command->run(context);
    return true;
}
