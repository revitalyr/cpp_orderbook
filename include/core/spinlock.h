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
private: // Internal state // Renamed to camelCase
    std::atomic_flag m_mutex = ATOMIC_FLAG_INIT; // Atomic flag for the spinlock // Renamed to m_snake_case
public: // Public interface // Renamed to camelCase
    void lock() { // Acquires the lock, spins if unavailable // Renamed to camelCase
        while(true) {
            while(m_mutex.test(std::memory_order_acquire)) { // Renamed to m_snake_case
                #if defined(__aarch64__)
                                asm volatile("yield" ::: "memory");
                #else
                                _mm_pause();
                #endif
            }
            if(!m_mutex.test_and_set(std::memory_order_acquire)) { // Renamed to m_snake_case
                return;
            } else {
                std::this_thread::yield();
            }
        }
    }
    bool tryLock() { // Attempts to acquire the lock without spinning // Renamed to camelCase
        return !m_mutex.test_and_set(std::memory_order_acquire); // Renamed to m_snake_case
    }
    void unlock() { // Releases the lock // Renamed to camelCase
        m_mutex.clear(std::memory_order_release); // Renamed to m_snake_case
    }
    bool isLocked() { // Checks if the lock is currently held // Renamed to camelCase
        return m_mutex.test(); // Renamed to m_snake_case
    }
};

typedef std::lock_guard<SpinLock> Guard;
} // namespace orderbook