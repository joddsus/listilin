#pragma once

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;
class QCloseEvent;

// A small window that says what a long operation is doing right now, how far it
// has come, and lets the user stop it. Closing it cancels, which is what the
// closed window implies.
class OperationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OperationDialog(const QString &title, QWidget *parent = nullptr);

    void showProgress(qint64 bytes, qint64 total, const QString &current, int items);
    void showCancelling();
    void setPaused(bool paused);

signals:
    void cancelRequested();
    void pauseRequested(bool paused);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QLabel *m_current = nullptr;
    QLabel *m_amount = nullptr;
    QProgressBar *m_bar = nullptr;
    QPushButton *m_pause = nullptr;
    QPushButton *m_cancel = nullptr;
    QString m_cancelText;
    bool m_cancelling = false;
    bool m_paused = false;
};
