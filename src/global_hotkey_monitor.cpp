#include "global_hotkey_monitor.h"

#include <QCoreApplication>
#include <QKeyCombination>

#include <windows.h>

#include "logger.h"

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

namespace {

QString WindowsErrorMessage(const QString& action, DWORD errorCode) {
    return QString::fromUtf8("%1失败（Windows 错误码 %2）")
        .arg(action)
        .arg(static_cast<unsigned long>(errorCode));
}

bool IsReservedApplicationKey(unsigned int virtualKey) {
    return virtualKey == VK_SCROLL || virtualKey == VK_PAUSE ||
        virtualKey == VK_INSERT || virtualKey == VK_HOME;
}

unsigned int ToWindowsVirtualKey(Qt::Key key) {
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return VK_F1 + static_cast<unsigned int>(key - Qt::Key_F1);
    }
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return 'A' + static_cast<unsigned int>(key - Qt::Key_A);
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return '0' + static_cast<unsigned int>(key - Qt::Key_0);
    }

    switch (key) {
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Tab: return VK_TAB;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Return: return VK_RETURN;
    case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_QuoteLeft: return VK_OEM_3;
    case Qt::Key_Minus: return VK_OEM_MINUS;
    case Qt::Key_Equal: return VK_OEM_PLUS;
    case Qt::Key_BracketLeft: return VK_OEM_4;
    case Qt::Key_Backslash: return VK_OEM_5;
    case Qt::Key_BracketRight: return VK_OEM_6;
    case Qt::Key_Semicolon: return VK_OEM_1;
    case Qt::Key_Apostrophe: return VK_OEM_7;
    case Qt::Key_Comma: return VK_OEM_COMMA;
    case Qt::Key_Period: return VK_OEM_PERIOD;
    case Qt::Key_Slash: return VK_OEM_2;
    default: return 0;
    }
}

}

GlobalHotkeyMonitor::GlobalHotkeyMonitor(QObject* parent)
    : QObject(parent) {
    m_releaseTimer.setInterval(16);
    m_releaseTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_releaseTimer, &QTimer::timeout, this, &GlobalHotkeyMonitor::pollRelease);

    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
}

GlobalHotkeyMonitor::~GlobalHotkeyMonitor() {
    m_releaseTimer.stop();
    unregisterCurrentShortcut();
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

bool GlobalHotkeyMonitor::setShortcut(const QKeySequence& shortcut, QString* errorMessage) {
    unsigned int candidateModifiers = 0;
    unsigned int candidateVirtualKey = 0;
    QString normalizedText;
    QString validationError;
    if (!convertShortcut(shortcut, candidateModifiers, candidateVirtualKey, normalizedText, validationError)) {
        if (errorMessage) {
            *errorMessage = validationError;
        }
        return false;
    }

    const QKeySequence normalizedShortcut = QKeySequence::fromString(normalizedText, QKeySequence::PortableText);
    if (candidateModifiers == m_modifiers && candidateVirtualKey == m_virtualKey &&
        normalizedShortcut == m_shortcut) {
        return true;
    }

    const QKeySequence previousShortcut = m_shortcut;
    const unsigned int previousModifiers = m_modifiers;
    const unsigned int previousVirtualKey = m_virtualKey;
    const bool keepRegistered = m_enabled && !m_captureSuspended;

    finishActivePress();
    unregisterCurrentShortcut();

    m_shortcut = normalizedShortcut;
    m_modifiers = candidateModifiers;
    m_virtualKey = candidateVirtualKey;

    QString registrationError;
    if (!registerCurrentShortcut(&registrationError)) {
        m_shortcut = previousShortcut;
        m_modifiers = previousModifiers;
        m_virtualKey = previousVirtualKey;
        if (keepRegistered && m_virtualKey != 0) {
            QString rollbackError;
            if (!registerCurrentShortcut(&rollbackError)) {
                Logger::error("Failed to restore previous quick-panel hotkey: {}",
                    rollbackError.toStdString());
            }
        }
        if (errorMessage) {
            *errorMessage = registrationError;
        }
        return false;
    }

    if (!keepRegistered) {
        unregisterCurrentShortcut();
    }

    Logger::info("Quick-panel hotkey updated: shortcut={}", normalizedText.toStdString());
    return true;
}

bool GlobalHotkeyMonitor::setEnabled(bool enabled, QString* errorMessage) {
    if (m_enabled == enabled) {
        if (!enabled || m_captureSuspended || m_registered) {
            return true;
        }
    }

    if (!enabled) {
        finishActivePress();
        unregisterCurrentShortcut();
        m_enabled = false;
        Logger::info("Quick-panel hotkey disabled.");
        return true;
    }

    if (m_virtualKey == 0) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("请先设置有效的快捷键");
        }
        return false;
    }

    m_enabled = true;
    if (!m_captureSuspended && !registerCurrentShortcut(errorMessage)) {
        m_enabled = false;
        return false;
    }

    Logger::info("Quick-panel hotkey enabled: shortcut={}",
        m_shortcut.toString(QKeySequence::PortableText).toStdString());
    return true;
}

bool GlobalHotkeyMonitor::setCaptureSuspended(bool suspended, QString* errorMessage) {
    if (m_captureSuspended == suspended) {
        return true;
    }

    m_captureSuspended = suspended;
    if (suspended) {
        finishActivePress();
        unregisterCurrentShortcut();
        Logger::debug("Quick-panel hotkey temporarily suspended for shortcut capture.");
        return true;
    }

    if (m_enabled && !registerCurrentShortcut(errorMessage)) {
        Logger::warn("Quick-panel hotkey could not resume after shortcut capture.");
        return false;
    }

    Logger::debug("Quick-panel hotkey resumed after shortcut capture.");
    return true;
}

QKeySequence GlobalHotkeyMonitor::shortcut() const {
    return m_shortcut;
}

bool GlobalHotkeyMonitor::isEnabled() const {
    return m_enabled;
}

bool GlobalHotkeyMonitor::nativeEventFilter(const QByteArray&, void* message, qintptr* result) {
    auto* nativeMessage = static_cast<MSG*>(message);
    if (!nativeMessage || nativeMessage->message != WM_HOTKEY ||
        nativeMessage->wParam != static_cast<WPARAM>(kHotkeyId)) {
        return false;
    }

    if (!m_pressed && m_enabled && !m_captureSuspended) {
        m_pressed = true;
        m_releaseTimer.start();
        Logger::info("Quick-panel hotkey pressed: shortcut={}",
            m_shortcut.toString(QKeySequence::PortableText).toStdString());
        emit pressed();
    }

    if (result) {
        *result = 0;
    }
    return true;
}

void GlobalHotkeyMonitor::pollRelease() {
    if (m_pressed && !isShortcutDown()) {
        finishActivePress();
    }
}

bool GlobalHotkeyMonitor::convertShortcut(const QKeySequence& shortcut, unsigned int& modifiers,
    unsigned int& virtualKey, QString& normalizedText, QString& errorMessage) {
    if (shortcut.isEmpty() || shortcut.count() != 1) {
        errorMessage = QString::fromUtf8("快捷键必须包含且只能包含一个组合");
        return false;
    }

    const QKeyCombination combination = shortcut[0];
    const Qt::KeyboardModifiers qtModifiers = combination.keyboardModifiers();
    if (qtModifiers.testFlag(Qt::AltModifier)) {
        errorMessage = QString::fromUtf8("Alt 已用于自动标记，不能用于呼出控制面板");
        return false;
    }
    if (qtModifiers.testFlag(Qt::MetaModifier)) {
        errorMessage = QString::fromUtf8("Windows 键组合由系统保留，请选择其他快捷键");
        return false;
    }

    const Qt::KeyboardModifiers supportedModifiers = Qt::ControlModifier | Qt::ShiftModifier;
    if ((qtModifiers & ~supportedModifiers) != Qt::NoModifier) {
        errorMessage = QString::fromUtf8("仅支持 Ctrl、Shift 与一个普通按键组成快捷键");
        return false;
    }

    modifiers = 0;
    if (qtModifiers.testFlag(Qt::ControlModifier)) {
        modifiers |= MOD_CONTROL;
    }
    if (qtModifiers.testFlag(Qt::ShiftModifier)) {
        modifiers |= MOD_SHIFT;
    }

    virtualKey = ToWindowsVirtualKey(combination.key());
    if (virtualKey == 0) {
        errorMessage = QString::fromUtf8("当前按键暂不支持作为全局快捷键");
        return false;
    }
    if (IsReservedApplicationKey(virtualKey)) {
        errorMessage = QString::fromUtf8("该按键已用于资源路线功能，请选择其他按键");
        return false;
    }

    const bool isFunctionKey = virtualKey >= VK_F1 && virtualKey <= VK_F24;
    if (modifiers == 0 && !isFunctionKey) {
        errorMessage = QString::fromUtf8("为避免游戏误触，单键模式仅支持 F1 至 F24");
        return false;
    }

    normalizedText = shortcut.toString(QKeySequence::PortableText);
    return true;
}

bool GlobalHotkeyMonitor::registerCurrentShortcut(QString* errorMessage) {
    if (m_registered) {
        return true;
    }
    if (m_virtualKey == 0) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("快捷键尚未配置");
        }
        return false;
    }

    if (!RegisterHotKey(nullptr, kHotkeyId, m_modifiers | MOD_NOREPEAT, m_virtualKey)) {
        const DWORD errorCode = GetLastError();
        const QString error = WindowsErrorMessage(QString::fromUtf8("注册全局快捷键"), errorCode);
        Logger::warn("Quick-panel hotkey registration failed: shortcut={} error_code={}",
            m_shortcut.toString(QKeySequence::PortableText).toStdString(),
            static_cast<unsigned long>(errorCode));
        if (errorMessage) {
            *errorMessage = error;
        }
        return false;
    }

    m_registered = true;
    return true;
}

void GlobalHotkeyMonitor::unregisterCurrentShortcut() {
    if (!m_registered) {
        return;
    }
    UnregisterHotKey(nullptr, kHotkeyId);
    m_registered = false;
}

bool GlobalHotkeyMonitor::isShortcutDown() const {
    if ((m_modifiers & MOD_CONTROL) != 0 && (GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0) {
        return false;
    }
    if ((m_modifiers & MOD_SHIFT) != 0 && (GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0) {
        return false;
    }
    return (GetAsyncKeyState(static_cast<int>(m_virtualKey)) & 0x8000) != 0;
}

void GlobalHotkeyMonitor::finishActivePress() {
    if (!m_pressed) {
        return;
    }

    m_pressed = false;
    m_releaseTimer.stop();
    Logger::info("Quick-panel hotkey released.");
    emit released();
}
