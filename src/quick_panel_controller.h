#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <windows.h>

class QuickPanelController final : public QObject {
    Q_OBJECT

public:
    explicit QuickPanelController(QWidget* panel, QObject* parent = nullptr);

public slots:
    void showForShortcut();
    void minimizeAfterShortcut();

private:
    bool isPanelOwnedWindow(HWND window) const;
    bool activateWindow(HWND window) const;
    void completeMinimize();

    QPointer<QWidget> m_panel;
    QTimer m_pendingMinimizeTimer;
    QElapsedTimer m_pendingMinimizeElapsed;
    HWND m_previousForeground = nullptr;
    bool m_sessionActive = false;
};
