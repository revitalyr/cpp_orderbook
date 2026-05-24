#pragma once

#include <atomic>
#include <mutex>

#if defined(__aarch64__)
#else
#include <emmintrin.h>
#include <immintrin.h>
#include <xmmintrin.h>
#endif

namespace orderbook {

class SpinLock {
private:
    std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;
public:
    void lock() {
        while(true) {
            while(m_mutex.test(std::memory_order_acquire)) {
                #if defined(__aarch64__)
                                asm volatile("yield" ::: "memory");
                #else
                                _mm_pause();
                #endif
            }
            if(!m_mutex.test_and_set(std::memory_order_acquire)) {
                return;
            } else {
                std::this_thread::yield();
            }
        }
    }
    bool tryLock() {
        return !m_mutex.test_and_set(std::memory_order_acquire);
    }
    void unlock() {
        m_mutex.clear(std::memory_order_release);
    }
    bool isLocked() {
        return m_mutex.test();
    }
};

using Guard = std::lock_guard<SpinLock>;
} // namespace orderbook