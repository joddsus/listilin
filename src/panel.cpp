#include "panel.h"
#include "dirmodel.h"
#include "format.h"
#include "listerheader.h"
#include "listerview.h"
#include "vfs.h"

#include <QDir>
#include <QEvent>
#include <QFrame>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QStorageInfo>
#include <QVBoxLayout>

Panel::Panel(QWidget *parent)
    : QWidget(parent)
{
    m_frame = new QFrame(this);
    m_frame->setObjectName(QStringLiteral("listerFrame"));

    m_pathBar = new QLineEdit(m_frame);
    m_pathBar->setFrame(false);
    m_pathBar->setPlaceholderText(tr("Path or address, e.g. ftp://host/folder"));
    m_pathBar->setStyleSheet(QStringLiteral(
        "QLineEdit { padding: 4px 8px; border: none; background: transparent; }"));

    m_model = new DirModel(this);
    m_view = new ListerView(m_frame);
    m_view->setModel(m_model);
    m_header = new ListerHeader(m_view, m_frame);

    m_status = new QLabel(m_frame);
    QFont statusFont = m_status->font();
    statusFont.setPointSizeF(statusFont.pointSizeF() * 0.9);
    m_status->setFont(statusFont);
    m_status->setContentsMargins(8, 3, 8, 3);

    auto *layout = new QVBoxLayout(m_frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_pathBar);
    layout->addWidget(m_header);
    layout->addWidget(m_view, 1);
    layout->addWidget(m_status);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_frame);

    setFocusProxy(m_view);

    connect(m_pathBar, &QLineEdit::returnPressed, this, [this]() {
        const QString entered = m_pathBar->text().trimmed();
        if (!entered.isEmpty())
            setUri(entered);
        focusLister();
    });
    connect(m_model, &DirModel::uriChanged, this, [this](const QString &uri) {
        m_pathBar->setText(displayFor(uri));
        m_pathBar->setCursorPosition(m_pathBar->text().size());
        recordHistory(uri);
        emit uriChanged(uri);
    });
    connect(m_model, &DirModel::loaded, this, [this](int) { updateStatus(); });
    connect(m_model, &DirModel::loadFailed, this, [this](const QString &error) {
        m_status->setText(error);
    });
    connect(m_model, &DirModel::sortChanged, m_header, &ListerHeader::setSortIndicator);
    connect(m_header, &ListerHeader::sortRequested, this, [this](int column) {
        m_model->setSort(column, m_model->sortOrder());
        m_header->setSortIndicator(m_model->sortColumn(), m_model->sortOrder());
    });
    connect(m_view, &ListerView::commandRequested, this, &Panel::commandRequested);
    connect(m_view, &ListerView::itemActivated, this, [this](const QModelIndex &) {
        emit commandRequested(QStringLiteral("lister.open"));
    });
    connect(m_view, &ListerView::contextMenuRequested, this, &Panel::contextMenuRequested);
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection &, const QItemSelection &) { updateStatus(); });

    // A lister with nothing in it should say so, not look broken.
    m_empty = new QLabel(m_view->viewport());
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setAttribute(Qt::WA_TransparentForMouseEvents);
    QPalette emptyPalette = m_empty->palette();
    emptyPalette.setColor(QPalette::WindowText, emptyPalette.color(QPalette::Mid));
    m_empty->setPalette(emptyPalette);
    m_empty->hide();
    m_view->viewport()->installEventFilter(this);

    connect(m_model, &DirModel::loaded, this, [this](int) { updateEmptyState(); });
    connect(m_model, &DirModel::loadFailed, this, [this](const QString &) { updateEmptyState(); });
    connect(m_model, &DirModel::uriChanged, this, [this](const QString &) { updateEmptyState(); });

    m_header->setSortIndicator(m_model->sortColumn(), m_model->sortOrder());
    applyActiveStyle();
    updateStatus();
}

QString Panel::displayFor(const QString &uri)
{
    const VfsUri parsed = VfsUri::parse(uri);
    return parsed.isLocal() ? parsed.path : parsed.toString();
}

QString Panel::uri() const
{
    return m_model->uri();
}

void Panel::setUri(const QString &uri)
{
    m_model->setUri(uri);
}

void Panel::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    applyActiveStyle();
    updateStatus();
}

void Panel::applyActiveStyle()
{
    const QColor accent = palette().color(m_active ? QPalette::Highlight : QPalette::Mid);
    m_frame->setStyleSheet(QStringLiteral("QFrame#listerFrame { border: 1px solid %1; }")
                               .arg(accent.name()));

    QPalette barPalette = m_pathBar->palette();
    barPalette.setColor(QPalette::Base, palette().color(QPalette::Window));
    m_pathBar->setPalette(barPalette);
}

QString Panel::currentName() const
{
    return m_view->currentName();
}

QString Panel::currentUri() const
{
    const QModelIndex index = m_view->currentIndex();
    if (!index.isValid())
        return uri();
    return index.data(DirModel::UriRole).toString();
}

QStringList Panel::selection() const
{
    QStringList names;
    const QModelIndexList indexes = m_view->selectionModel()->selectedIndexes();
    names.reserve(indexes.size());
    for (const QModelIndex &index : indexes)
        names.append(index.data(Qt::DisplayRole).toString());
    return names;
}

QStringList Panel::selectedUris() const
{
    QStringList uris;
    const QModelIndexList indexes = m_view->selectionModel()->selectedIndexes();
    uris.reserve(indexes.size());
    for (const QModelIndex &index : indexes)
        uris.append(index.data(DirModel::UriRole).toString());
    return uris;
}

void Panel::focusPathBar()
{
    m_pathBar->setFocus(Qt::OtherFocusReason);
    m_pathBar->selectAll();
}

void Panel::focusLister()
{
    m_view->setFocus(Qt::OtherFocusReason);
    if (!m_view->currentIndex().isValid() && m_model->rowCount() > 0)
        m_view->setCurrentIndex(m_model->index(0, 0));
}

void Panel::selectName(const QString &name)
{
    const int row = m_model->indexOf(name);
    if (row < 0)
        return;

    const QModelIndex index = m_model->index(row, 0);
    m_view->setCurrentIndex(index);
    m_view->scrollTo(index, QAbstractItemView::EnsureVisible);
}

void Panel::recordHistory(const QString &uri)
{
    if (m_navigatingHistory)
        return;

    if (m_historyIndex >= 0 && m_history.value(m_historyIndex) == uri)
        return;

    while (m_history.size() > m_historyIndex + 1)
        m_history.removeLast();

    m_history.append(uri);
    m_historyIndex = m_history.size() - 1;
}

bool Panel::canGoBack() const
{
    return m_historyIndex > 0;
}

bool Panel::canGoForward() const
{
    return m_historyIndex >= 0 && m_historyIndex + 1 < m_history.size();
}

void Panel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_empty)
        m_empty->setGeometry(m_view->viewport()->rect());
}

bool Panel::eventFilter(QObject *watched, QEvent *event)
{
    if (m_empty && watched == m_view->viewport() && event->type() == QEvent::Resize)
        m_empty->setGeometry(m_view->viewport()->rect());

    return QWidget::eventFilter(watched, event);
}

void Panel::updateEmptyState()
{
    if (!m_empty)
        return;

    const QString error = m_model->lastError();
    if (!error.isEmpty()) {
        m_empty->setText(error);
        m_empty->show();
    } else if (m_model->rowCount() == 0) {
        m_empty->setText(tr("Nothing here"));
        m_empty->show();
    } else {
        m_empty->hide();
    }

    m_empty->setGeometry(m_view->viewport()->rect());
}

void Panel::goBack()
{
    if (!canGoBack())
        return;

    m_navigatingHistory = true;
    --m_historyIndex;
    m_model->setUri(m_history.at(m_historyIndex));
    m_navigatingHistory = false;
}

void Panel::goForward()
{
    if (!canGoForward())
        return;

    m_navigatingHistory = true;
    ++m_historyIndex;
    m_model->setUri(m_history.at(m_historyIndex));
    m_navigatingHistory = false;
}

void Panel::updateStatus()
{
    const int count = m_model->rowCount();
    const int selected = m_view->selectionModel()->selectedIndexes().size();

    QStringList parts;
    parts << tr("%1 items").arg(count);

    if (selected > 0) {
        qint64 bytes = 0;
        const QModelIndexList indexes = m_view->selectionModel()->selectedIndexes();
        for (const QModelIndex &index : indexes)
            bytes += index.data(DirModel::SizeBytesRole).toLongLong();
        parts << tr("%1 selected · %2").arg(selected).arg(humanSize(bytes));
    }

    if (!Vfs::instance().writable(uri()))
        parts << tr("read-only");

    if (Vfs::isLocal(uri())) {
        const QStorageInfo storage(Vfs::localPathOf(uri()));
        if (storage.isValid() && storage.isReady())
            parts << tr("%1 free").arg(humanSize(storage.bytesAvailable()));
    }

    m_status->setText(parts.join(QStringLiteral("  ·  ")));
}
