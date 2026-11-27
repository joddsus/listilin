#include "folderwatcher.h"

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

FolderWatcher::FolderWatcher(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(250);

    connect(m_timer, &QTimer::timeout, this, [this]() {
        if (!m_folder.isEmpty())
            emit folderChanged(m_folder);
    });
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString &) { m_timer->start(); });
}

void FolderWatcher::setFolder(const QString &folder)
{
    if (folder == m_folder)
        return;

    if (!m_folder.isEmpty())
        m_watcher->removePath(m_folder);
    m_folder = folder;
    m_timer->stop();

    if (!m_folder.isEmpty() && QFileInfo(m_folder).isDir())
        m_watcher->addPath(m_folder);
}
