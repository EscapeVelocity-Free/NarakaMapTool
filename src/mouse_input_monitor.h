#ifndef MOUSE_INPUT_MONITOR_H
#define MOUSE_INPUT_MONITOR_H

#include <QObject>
#include <QTimer>
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <windows.h>

class MouseInputMonitor : public QObject {
    Q_OBJECT

public:
    explicit MouseInputMonitor(QObject* parent = nullptr);
    ~MouseInputMonitor() override;

    bool start();
    void stop();

signals:
    void wheelChanged(int wheelDelta, int screenX, int screenY, bool injected);

private:
    struct WheelEvent {
        int delta = 0;
        int screenX = 0;
        int screenY = 0;
    };

    static constexpr size_t kWheelQueueCapacity = 128;

    void flushWheel();
    bool enqueueWheel(const WheelEvent& event);
    void runHookThread();
    static LRESULT CALLBACK mouseHookProc(int code, WPARAM wParam, LPARAM lParam);

    static std::atomic<MouseInputMonitor*> s_instance;
    QTimer m_flushTimer;
    std::array<WheelEvent, kWheelQueueCapacity> m_wheelQueue{};
    std::atomic<size_t> m_wheelWriteIndex{0};
    std::atomic<size_t> m_wheelReadIndex{0};
    std::atomic<uint64_t> m_droppedWheelEvents{0};
    // 唤醒协调：生产者入队后置位并排队 start；消费者清空后先复位再复查队列，
    // 保证"停止定时器"与"新事件入队"交错时不会丢失唤醒（性能优化）。
    std::atomic<bool> m_wakePending{false};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
    std::thread m_hookThread;
    std::mutex m_hookStateMutex;
    std::condition_variable m_hookStateChanged;
    bool m_hookReady = false;
    bool m_hookInstalled = false;
    DWORD m_hookThreadId = 0;
};

#endif
