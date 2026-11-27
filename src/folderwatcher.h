#pragma once

#include <QObject>
#include <QString>

class QFileSystemWatcher;
class QTimer;

// Watches one local folder and reports when its contents may have changed.
// Notifications are coalesced into a single signal so a copy or extraction
// that creates many entries causes one reload, not one per file.
class FolderWatcher : public QObject
{
    Q_OBJECT

public:
    explicit FolderWatcher(QObject *parent = nullptr);

    // Watches `folder`, or stops watching when it is empty or not a folder.
    void setFolder(const QString &folder);
    QString folder() const { return m_folder; }

signals:
    void folderChanged(const QString &folder);

private:
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_timer = nullptr;
    QString m_folder;
};
