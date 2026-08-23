#include "quick_panel_controller.h"

#include "logger.h"

namespace {
constexpr int kMouseReleasePollMs = 16;
constexpr int kMaximumMouseReleaseWaitMs = 500;
}

QuickPanelController::QuickPanelController(QWidget* panel, QObject* parent)
    : QObject(parent), m_panel(panel) {
    m_pendingMinimizeTimer.setInterval(kMouseReleasePollMs);
    m_pendingMinimizeTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_pendingMinimizeTimer, &QTimer::timeout, this, [this]() {
        const bool leftMouseReleased = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0;
        if (leftMouseReleased || m_pendingMinimizeElapsed.elapsed() >= kMaximumMouseReleaseWaitMs) {
            completeMinimize();
        }
    });
}

void QuickPanelController::showForShortcut() {
    if (!m_panel || m_sessionActive) {
        return;
    }

    const HWND panelWindow = reinterpret_cast<HWND>(m_panel->winId());
    const HWND foregroundWindow = GetForegroundWindow();
    if (isPanelOwnedWindow(foregroundWindow)) {
        Logger::debug("Quick-panel request ignored because the control panel is already foreground.");
        return;
    }

    m_pendingMinimizeTimer.stop();
    m_previousForeground = foregroundWindow;
    m_sessionActive = true;

    m_panel->showNormal();
    SetWindowPos(panelWindow, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    m_panel->raise();
    m_panel->activateWindow();

    const bool activated = activateWindow(panelWindow);
    Logger::info("Control panel shown for quick access: previous_foreground={} panel={} activated={}",
        static_cast<const void*>(m_previousForeground), static_cast<const void*>(panelWindow), activated);
}

void QuickPanelController::minimizeAfterShortcut() {
    if (!m_sessionActive) {
        return;
    }

    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) {
        m_pendingMinimizeElapsed.restart();
        m_pendingMinimizeTimer.start();
        Logger::debug("Control panel minimize deferred until the active mouse click completes.");
        return;
    }

    completeMinimize();
}

bool QuickPanelController::isPanelOwnedWindow(HWND window) const {
    if (!m_panel || !window) {
        return false;
    }

    const HWND panelWindow = reinterpret_cast<HWND>(m_panel->winId());
    return window == panelWindow || IsChild(panelWindow, window) ||
        GetAncestor(window, GA_ROOTOWNER) == panelWindow;
}

bool QuickPanelController::activateWindow(HWND window) const {
    if (!window || !IsWindow(window)) {
        return false;
    }

    if (SetForegroundWindow(window)) {
        return true;
    }

    const HWND foregroundWindow = GetForegroundWindow();
    const DWORD foregroundThread = foregroundWindow
        ? GetWindowThreadProcessId(foregroundWindow, nullptr)
        : 0;
    const DWORD currentThread = GetCurrentThreadId();
    const bool attached = foregroundThread != 0 && foregroundThread != currentThread &&
        AttachThreadInput(currentThread, foregroundThread, TRUE) != FALSE;

    BringWindowToTop(window);
    const bool activated = SetForegroundWindow(window) != FALSE;
    if (attached) {
        AttachThreadInput(currentThread, foregroundThread, FALSE);
    }
    return activated;
}

void QuickPanelController::completeMinimize() {
    if (!m_sessionActive) {
        return;
    }

    m_pendingMinimizeTimer.stop();

    const HWND panelWindow = m_panel
        ? reinterpret_cast<HWND>(m_panel->winId())
        : nullptr;
    const HWND currentForeground = GetForegroundWindow();
    const bool shouldRestoreForeground = isPanelOwnedWindow(currentForeground);
    const HWND previousForeground = m_previousForeground;

    if (m_panel && panelWindow) {
        SetWindowPos(panelWindow, HWND_NOTOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        m_panel->showMinimized();
    }

    m_sessionActive = false;
    m_previousForeground = nullptr;

    bool restored = false;
    if (shouldRestoreForeground && previousForeground && previousForeground != panelWindow &&
        IsWindow(previousForeground)) {
        if (IsIconic(previousForeground)) {
            ShowWindow(previousForeground, SW_RESTORE);
        }
        restored = activateWindow(previousForeground);
    }

    Logger::info("Control panel minimized after quick access: restored_previous_foreground={}", restored);
}
