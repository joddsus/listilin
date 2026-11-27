#include "shellwidget.h"

#include "shellsession.h"
#include "vfs.h"

#include <QDir>
#include <QEvent>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

ShellWidget::ShellWidget(const QString &workingDirectory, QWidget *parent)
    : QWidget(parent)
    , m_session(new ShellSession(workingDirectory, this))
{
    m_prompt = new QLabel(this);
    m_prompt->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont promptFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    promptFont.setBold(true);
    m_prompt->setFont(promptFont);

    m_hint = new QLabel(this);
    m_hint->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); }"));

    auto *clearButton = new QPushButton(tr("Clear"), this);
    m_stop = new QPushButton(tr("Stop"), this);
    m_stop->setEnabled(false);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(6, 4, 6, 0);
    header->setSpacing(8);
    header->addWidget(m_prompt);
    header->addWidget(m_hint, 1);
    header->addWidget(clearButton);
    header->addWidget(m_stop);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(5000);
    m_output->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_output->setFrameShape(QFrame::NoFrame);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Command in this folder…"));
    m_input->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_input->installEventFilter(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addLayout(header);
    layout->addWidget(m_output, 1);
    layout->addWidget(m_input);

    connect(m_input, &QLineEdit::returnPressed, this, &ShellWidget::submit);
    connect(clearButton, &QPushButton::clicked, this, [this]() { m_output->clear(); });
    connect(m_stop, &QPushButton::clicked, this, [this]() { m_session->cancel(); });
    connect(m_session, &ShellSession::output, this, &ShellWidget::appendOutput);
    connect(m_session, &ShellSession::workingDirectoryChanged, this,
            [this](const QString &) { updatePrompt(); });
    connect(m_session, &ShellSession::busyChanged, this, [this](bool busy) {
        m_stop->setEnabled(busy);
        if (!busy)
            m_input->setFocus();
    });

    updatePrompt();
}

void ShellWidget::updatePrompt()
{
    m_prompt->setText(m_session->workingDirectory() + QStringLiteral(" $"));
    m_prompt->setToolTip(m_session->workingDirectory());
}

void ShellWidget::followUri(const QString &uri)
{
    if (Vfs::isLocal(uri)) {
        m_hint->clear();
        m_session->setWorkingDirectory(Vfs::localPathOf(uri));
        return;
    }

    m_hint->setText(tr("Shell stays in the last local folder while this location is shown."));
}

void ShellWidget::focusInput()
{
    m_input->setFocus();
}

void ShellWidget::submit()
{
    const QString command = m_input->text().trimmed();
    if (command.isEmpty())
        return;

    m_input->clear();
    if (command == QLatin1String("clear") || command == QLatin1String("cls")) {
        m_output->clear();
        return;
    }

    appendOutput(QStringLiteral("%1 %2\n").arg(m_prompt->text(), command));

    if (m_history.isEmpty() || m_history.last() != command)
        m_history.append(command);
    m_historyIndex = m_history.size();
    m_draft.clear();

    m_session->run(command);
}

void ShellWidget::appendOutput(const QString &text)
{
    if (text.isEmpty())
        return;
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
    m_output->moveCursor(QTextCursor::End);
}

void ShellWidget::recallHistory(int delta)
{
    if (m_history.isEmpty())
        return;

    if (m_historyIndex == m_history.size() && delta < 0)
        m_draft = m_input->text();

    int next = m_historyIndex + delta;
    next = qBound(0, next, m_history.size());

    m_historyIndex = next;
    if (next == m_history.size())
        m_input->setText(m_draft);
    else
        m_input->setText(m_history.at(next));
    m_input->setCursorPosition(m_input->text().size());
}

bool ShellWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Up) {
            recallHistory(-1);
            return true;
        }
        if (key->key() == Qt::Key_Down) {
            recallHistory(1);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
