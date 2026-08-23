#pragma once

#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QObject>
#include <QTimer>

class GlobalHotkeyMonitor final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit GlobalHotkeyMonitor(QObject* parent = nullptr);
    ~GlobalHotkeyMonitor() override;

    bool setShortcut(const QKeySequence& shortcut, QString* errorMessage = nullptr);
    bool setEnabled(bool enabled, QString* errorMessage = nullptr);
    bool setCaptureSuspended(bool suspended, QString* errorMessage = nullptr);

    QKeySequence shortcut() const;
    bool isEnabled() const;

signals:
    void pressed();
    void released();

protected:
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private slots:
    void pollRelease();

private:
    static bool convertShortcut(const QKeySequence& shortcut, unsigned int& modifiers,
        unsigned int& virtualKey, QString& normalizedText, QString& errorMessage);
    bool registerCurrentShortcut(QString* errorMessage = nullptr);
    void unregisterCurrentShortcut();
    bool isShortcutDown() const;
    void finishActivePress();

    static constexpr int kHotkeyId = 0x4E4D;

    QTimer m_releaseTimer;
    QKeySequence m_shortcut;
    unsigned int m_modifiers = 0;
    unsigned int m_virtualKey = 0;
    bool m_enabled = false;
    bool m_registered = false;
    bool m_captureSuspended = false;
    bool m_pressed = false;
};
