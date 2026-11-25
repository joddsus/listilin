#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class DirModel;
class ListerHeader;
class ListerView;
class MillerColumnsView;
class QFrame;
class QStackedWidget;
class QLabel;
class QLineEdit;

// One side of the window: an address bar, a lister and a status line. Two of
// these make the SRC/DEST pair that is the whole point of the layout.
class Panel : public QWidget
{
    Q_OBJECT

public:
    explicit Panel(QWidget *parent = nullptr);

    DirModel *model() const { return m_model; }
    ListerView *view() const { return m_view; }

    QString uri() const;
    void setUri(const QString &uri);

    void setActive(bool active);
    bool isActive() const { return m_active; }

    bool iconView() const;
    void setIconView(bool icons);
    void setPreviewVisible(bool visible);

    bool millerView() const;
    void setMillerView(bool miller);
    MillerColumnsView *millerColumnsView() const { return m_miller; }

    QStringList selection() const;
    QStringList selectedUris() const;
    QString currentName() const;
    QString currentUri() const;

    void focusPathBar();
    void focusLister();
    void focusFilterBar();
    void selectName(const QString &name);

    bool canGoBack() const;
    bool canGoForward() const;
    void goBack();
    void goForward();

    static QString displayFor(const QString &uri);

signals:
    void commandRequested(const QString &commandId);
    void uriChanged(const QString &uri);
    void contextMenuRequested(const QPoint &globalPos);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void recordHistory(const QString &uri);
    void updateStatus();
    void updateEmptyState();
    void updatePreview();
    void applyActiveStyle();

    QLabel *m_empty = nullptr;
    QLabel *m_preview = nullptr;

    QFrame *m_frame = nullptr;
    QLineEdit *m_pathBar = nullptr;
    QLineEdit *m_filterBar = nullptr;
    ListerHeader *m_header = nullptr;
    QStackedWidget *m_viewStack = nullptr;
    ListerView *m_view = nullptr;
    MillerColumnsView *m_miller = nullptr;
    DirModel *m_model = nullptr;
    QLabel *m_status = nullptr;
    bool m_active = false;

    QStringList m_history;
    int m_historyIndex = -1;
    bool m_navigatingHistory = false;
    bool m_syncingMiller = false;
};
