#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class ShellSession;

// The visible side of one panel's command line: a transcript, a prompt that
// names the folder, an input line with history, and a Stop button.
class ShellWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ShellWidget(const QString &workingDirectory, QWidget *parent = nullptr);

    ShellSession *session() const { return m_session; }
    void followUri(const QString &uri);
    void focusInput();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void submit();
    void appendOutput(const QString &text);
    void updatePrompt();
    void recallHistory(int delta);

    ShellSession *m_session = nullptr;
    QPlainTextEdit *m_output = nullptr;
    QLineEdit *m_input = nullptr;
    QLabel *m_prompt = nullptr;
    QLabel *m_hint = nullptr;
    QPushButton *m_stop = nullptr;

    QStringList m_history;
    int m_historyIndex = 0;
    QString m_draft;
};
