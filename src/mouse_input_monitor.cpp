#include "mouse_input_monitor.h"

#include <chrono>

#include "logger.h"

std::atomic<MouseInputMonitor*> MouseInputMonitor::s_instance{nullptr};

MouseInputMonitor::MouseInputMonitor(QObject* parent) : QObject(parent) {
    m_flushTimer.setInterval(2);
    m_flushTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_flushTimer, &QTimer::timeout, this, &MouseInputMonitor::flushWheel);
}

MouseInputMonitor::~MouseInputMonitor() {
    stop();
}

bool MouseInputMonitor::start() {
    if (m_running.load(std::memory_order_acquire)) {
        return true;
    }
    MouseInputMonitor* currentInstance = s_instance.load(std::memory_order_acquire);
    if (currentInstance && currentInstance != this) {
        Logger::warn("Mouse input monitor is already owned by another instance.");
        return false;
    }

    s_instance.store(this, std::memory_order_release);
    m_stopRequested.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(m_hookStateMutex);
        m_hookReady = false;
        m_hookInstalled = false;
        m_hookThreadId = 0;
    }

    m_hookThread = std::thread(&MouseInputMonitor::runHookThread, this);
    std::unique_lock<std::mutex> lock(m_hookStateMutex);
    const bool ready = m_hookStateChanged.wait_for(lock, std::chrono::seconds(2), [this]() {
        return m_hookReady;
    });
    const bool installed = ready && m_hookInstalled;
    lock.unlock();

    if (!installed) {
        Logger::error("Failed to install low-level mouse hook on the dedicated thread.");
        stop();
        return false;
    }

    m_running.store(true, std::memory_order_release);
    m_flushTimer.start();
    Logger::info("Low-level mouse wheel monitor started on a dedicated thread.");
    return true;
}

void MouseInputMonitor::stop() {
    m_running.store(false, std::memory_order_release);
    m_flushTimer.stop();
    m_wheelReadIndex.store(m_wheelWriteIndex.load(std::memory_order_acquire),
        std::memory_order_release);
    m_stopRequested.store(true, std::memory_order_release);

    DWORD hookThreadId = 0;
    {
        std::lock_guard<std::mutex> lock(m_hookStateMutex);
        hookThreadId = m_hookThreadId;
    }
    if (hookThreadId != 0) {
        PostThreadMessageW(hookThreadId, WM_QUIT, 0, 0);
    }
    if (m_hookThread.joinable()) {
        m_hookThread.join();
    }

    MouseInputMonitor* expected = this;
    s_instance.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
}

LRESULT CALLBACK MouseInputMonitor::mouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    MouseInputMonitor* instance = s_instance.load(std::memory_order_acquire);
    if (code == HC_ACTION && instance && instance->m_running.load(std::memory_order_acquire) &&
        wParam == WM_MOUSEWHEEL) {
        const auto* mouseData = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        const int wheelDelta = static_cast<short>(HIWORD(mouseData->mouseData));
        const bool injected = (mouseData->flags & LLMHF_INJECTED) != 0;
        if (!injected && wheelDelta != 0) {
            instance->enqueueWheel({wheelDelta, mouseData->pt.x, mouseData->pt.y});
        }
    }

    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void MouseInputMonitor::runHookThread() {
    const DWORD threadId = GetCurrentThreadId();
    MSG queueMessage{};
    PeekMessageW(&queueMessage, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    HHOOK hook = SetWindowsHookExW(
        WH_MOUSE_LL, &MouseInputMonitor::mouseHookProc, GetModuleHandleW(nullptr), 0);

    {
        std::lock_guard<std::mutex> lock(m_hookStateMutex);
        m_hookThreadId = threadId;
        m_hookInstalled = hook != nullptr;
        m_hookReady = true;
    }
    m_hookStateChanged.notify_one();

    if (!hook) {
        return;
    }

    MSG message{};
    while (!m_stopRequested.load(std::memory_order_acquire) &&
        GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnhookWindowsHookEx(hook);
}

bool MouseInputMonitor::enqueueWheel(const WheelEvent& event) {
    const size_t writeIndex = m_wheelWriteIndex.load(std::memory_order_relaxed);
    const size_t readIndex = m_wheelReadIndex.load(std::memory_order_acquire);
    if (writeIndex - readIndex >= kWheelQueueCapacity) {
        m_droppedWheelEvents.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    m_wheelQueue[writeIndex % kWheelQueueCapacity] = event;
    m_wheelWriteIndex.store(writeIndex + 1, std::memory_order_release);
    return true;
}

void MouseInputMonitor::flushWheel() {
    if (!m_running.load(std::memory_order_acquire)) {
        return;
    }

    size_t readIndex = m_wheelReadIndex.load(std::memory_order_relaxed);
    const size_t writeIndex = m_wheelWriteIndex.load(std::memory_order_acquire);
    while (readIndex < writeIndex) {
        const WheelEvent event = m_wheelQueue[readIndex % kWheelQueueCapacity];
        ++readIndex;
        emit wheelChanged(event.delta, event.screenX, event.screenY, false);
    }
    m_wheelReadIndex.store(readIndex, std::memory_order_release);

    const uint64_t dropped = m_droppedWheelEvents.exchange(0, std::memory_order_relaxed);
    if (dropped != 0) {
        Logger::warn("Mouse wheel queue overflowed; dropped_events={}", dropped);
    }
}
