#include "operationdialog.h"
#include "format.h"

#include <QCloseEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

OperationDialog::OperationDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
    , m_cancelText(tr("Cancel"))
{
    setWindowTitle(title);
    setWindowModality(Qt::WindowModal);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setSizeGripEnabled(false);
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 14);
    layout->setSpacing(10);

    auto *heading = new QLabel(title, this);
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    m_current = new QLabel(this);
    m_current->setTextFormat(Qt::PlainText);
    m_current->setMinimumWidth(400);
    layout->addWidget(m_current);

    m_bar = new QProgressBar(this);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(10);
    m_bar->setRange(0, 0);
    layout->addWidget(m_bar);

    auto *bottom = new QHBoxLayout;
    bottom->setSpacing(10);

    m_amount = new QLabel(this);
    bottom->addWidget(m_amount, 1);

    m_pause = new QPushButton(tr("Pause"), this);
    m_pause->setAutoDefault(false);
    m_pause->setDefault(false);
    bottom->addWidget(m_pause);

    m_cancel = new QPushButton(m_cancelText, this);
    m_cancel->setAutoDefault(false);
    m_cancel->setDefault(false);
    bottom->addWidget(m_cancel);

    layout->addLayout(bottom);

    connect(m_pause, &QPushButton::clicked, this, [this]() {
        if (m_cancelling)
            return;
        m_paused = !m_paused;
        m_pause->setText(m_paused ? tr("Resume") : tr("Pause"));
        emit pauseRequested(m_paused);
    });

    connect(m_cancel, &QPushButton::clicked, this, [this]() {
        if (m_cancelling)
            return;
        showCancelling();
        emit cancelRequested();
    });
}

void OperationDialog::showProgress(qint64 bytes, qint64 total, const QString &current, int items)
{
    if (m_cancelling)
        return;

    if (!current.isEmpty()) {
        const QFontMetrics metrics(m_current->font());
        m_current->setText(
            metrics.elidedText(current, Qt::ElideMiddle, qMax(360, m_current->width())));
        m_current->setToolTip(current);
    }

    if (total > 0) {
        m_bar->setRange(0, 1000);
        m_bar->setValue(int(qBound<qint64>(qint64(0), bytes * 1000 / total, qint64(1000))));
        m_amount->setText(tr("%1 of %2 · %3")
                              .arg(humanSize(bytes), humanSize(total), itemCount(items)));
    } else {
        // Unknown size: an honest indefinite bar instead of a fake percentage.
        m_bar->setRange(0, 0);
        m_amount->setText(tr("%1 · %2").arg(humanSize(bytes), itemCount(items)));
    }
}

void OperationDialog::showCancelling()
{
    m_cancelling = true;
    m_current->setText(tr("Cancelling …"));
    m_amount->setText(tr("Waiting for the current file to finish"));
    m_pause->setEnabled(false);
    m_cancel->setEnabled(false);
    m_cancel->setText(tr("Cancelling"));
}

void OperationDialog::setPaused(bool paused)
{
    m_paused = paused;
    if (m_pause)
        m_pause->setText(paused ? tr("Resume") : tr("Pause"));
    if (paused)
        m_current->setText(tr("Paused"));
}

void OperationDialog::closeEvent(QCloseEvent *event)
{
    if (!m_cancelling) {
        showCancelling();
        emit cancelRequested();
    }
    QDialog::closeEvent(event);
}
