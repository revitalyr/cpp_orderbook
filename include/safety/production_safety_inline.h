#pragma once

/**
 * Production Safety Module - Inline Implementation
 * 
 * Runtime protections for cpp_orderbook trading engine.
 * Prevents stack overflow during high-frequency trading operations.
 */

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <array>
#include "core/constants.h"

class ProductionSafety {
private:
    static inline std::atomic<bool> s_safetyEnabled{true}; // Flag to enable/disable safety features
    static inline std::atomic<int> s_failureCount{0}; // Counter for consecutive failures
    static inline std::atomic<std::chrono::steady_clock::time_point> s_lastFailureTime{
        std::chrono::steady_clock::now()
    };

public:
    // ====================================================================
    // Thread-local state (single source of truth for recursion depth)
    // ====================================================================
    struct ThreadLocalState {
        int m_recursionDepth = 0;
        uint32_t m_sampleCounter = 0;
        std::chrono::steady_clock::time_point m_lastResetTime = std::chrono::steady_clock::now();
    };
    
    [[nodiscard]] static inline ThreadLocalState& threadState() noexcept {
        thread_local ThreadLocalState state;
        return state;
    }
 
    // ====================================================================
    // Configuration
    // ====================================================================
    static inline void enableSafety(bool enabled = true) noexcept {
        s_safetyEnabled.store(enabled, std::memory_order_relaxed);
    }
    
    [[nodiscard]] static inline bool isTestMode() noexcept {
        return !s_safetyEnabled.load(std::memory_order_relaxed);
    }

    // ====================================================================
    // Recursion protection
    // ====================================================================
    [[nodiscard]] static inline bool enterCriticalOperation() noexcept {
        if (isTestMode()) return true;
        
        auto& state = threadState();
        
        // Sample clock only once every 512 calls to reduce syscall overhead
        if ((++state.m_sampleCounter & 511) == 0) {
            const auto now = std::chrono::steady_clock::now();
            if (now - state.m_lastResetTime > kResetInterval) {
                state.m_recursionDepth = 0;
                state.m_lastResetTime = now;
            }
        }
        
        if (++state.m_recursionDepth > kMaxRecursionDepth) {
            state.m_recursionDepth = 0;  // reset for recovery
            return false;
        }
        return true;
    }
    
    static inline void exitCriticalOperation() noexcept {
        if (isTestMode()) return;
        
        auto& state = threadState();
        if (state.m_recursionDepth > 0) {
            --state.m_recursionDepth;
        }
    }
 
    // ====================================================================
    // RAII Guard
    // ====================================================================
    class StackGuard {
    public:
        StackGuard() noexcept : m_isValid(enterCriticalOperation()) {}
        
        ~StackGuard() noexcept {
            if (m_isValid) {
                exitCriticalOperation();
            }
        }
        
        // Not copyable, not movable
        StackGuard(const StackGuard&) = delete;
        StackGuard& operator=(const StackGuard&) = delete;
        StackGuard(StackGuard&&) = delete;
        StackGuard& operator=(StackGuard&&) = delete;
        
        [[nodiscard]] bool isValid() const noexcept { return m_isValid; }
        
    private:
        bool m_isValid;
    };
 
    // ====================================================================
    // Circuit breaker
    // ====================================================================
    [[nodiscard]] static inline bool circuitBreakerAllow() noexcept {
        if (!s_safetyEnabled.load(std::memory_order_relaxed)) return true;
        
        const int failures = s_failureCount.load(std::memory_order_relaxed);
        if (failures > kFailureThreshold) {
            const auto last = s_lastFailureTime.load(std::memory_order_relaxed);
            const auto now = std::chrono::steady_clock::now();
            
            if (now - last > kCooldownPeriod) {
                s_failureCount.store(0, std::memory_order_relaxed);
                return true;
            }
            return false;  // Circuit open
        }
        return true;
    }
    
    static inline void recordFailure() noexcept {
        s_failureCount.fetch_add(1, std::memory_order_relaxed);
        s_lastFailureTime.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
    }
    
    static inline void recordSuccess() noexcept {
        int expected = s_failureCount.load(std::memory_order_relaxed);
        while (expected > 0) {
            if (s_failureCount.compare_exchange_weak(
                    expected, expected - 1,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }
    }
};
// ====================================================================
// Macros (safe for Boost.Test)
// ====================================================================

#define CRITICAL_OPERATION_GUARD_RAII \
    ProductionSafety::StackGuard stackGuardLocal; \
    if (!stackGuardLocal.isValid()) { \
        ProductionSafety::recordFailure(); \
        return; /* graceful early return instead of throw */ \
    }

#define CIRCUIT_BREAKER_CHECK \
    if (!ProductionSafety::circuitBreakerAllow()) { \
        ProductionSafety::recordFailure(); \
        return; /* graceful early return */ \
    }