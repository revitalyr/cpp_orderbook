#pragma once

#include <atomic>    // For std::atomic
#include <chrono>    // For std::chrono
#include <stdexcept> // For std::runtime_error
#include <thread>    // For std::thread
import orderbook.constants; // For kMaxRecursionDepth, kWarningThreshold, kResetInterval, kCooldownPeriod, kFailureThreshold

/**
 * Modern C++ Production Safety Module
 * 
 * Provides runtime protections for high-frequency trading systems with:
 * - Stack overflow prevention
 * - Circuit breaker pattern
 * - Thread-local state management
 * - Modern C++20 features and RAII
 */
class ProductionSafety {
public:
    // Modern type aliases
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::steady_clock::duration;
    
    // Configuration constants
    // kMaxRecursionDepth is defined in constants.h
    static constexpr Duration kResetInterval{std::chrono::seconds{1}}; // Renamed to kPascalCase
    static constexpr Duration kCooldownPeriod{std::chrono::seconds{30}};
    static constexpr int kFailureThreshold = 10; // Renamed to kPascalCase

private:
    // Thread-local state for recursion tracking
    struct ThreadLocalState {
        int m_recursionDepth{0}; // Renamed to m_snake_case
        TimePoint m_lastResetTime{std::chrono::steady_clock::now()}; // Renamed to m_snake_case
    };
    
    // Atomic state for circuit breaker
    static inline std::atomic<bool> s_safetyEnabled{true}; // Renamed to s_snake_case
    static inline std::atomic<int> s_failureCount{0}; // Renamed to s_snake_case
    static inline std::atomic<TimePoint> s_lastFailureTime{ // Renamed to s_snake_case
        std::chrono::steady_clock::now()
    };
    
    [[nodiscard]] static ThreadLocalState& getThreadState() noexcept {
        thread_local ThreadLocalState state;
        return state;
    }

public:
    // Configuration methods
    static void enableSafety(bool enabled = true) noexcept {
        s_safetyEnabled.store(enabled, std::memory_order_relaxed);
    }
    
    [[nodiscard]] static bool isTestMode() noexcept {
        return !s_safetyEnabled.load(std::memory_order_relaxed);
    }

    // Recursion protection
    [[nodiscard]] static bool enterCriticalOperation() noexcept {
        if (isTestMode()) {
            return true;
        }
        
        auto& state = getThreadState();
        const auto now = std::chrono::steady_clock::now();
        
        // Periodic reset to prevent permanent lockout
        if (now - state.m_lastResetTime > kResetInterval) { // Renamed to m_snake_case, kPascalCase
            state.m_recursionDepth = 0; // Renamed to m_snake_case
            state.m_lastResetTime = now; // Renamed to m_snake_case
        }
        
        if (++state.m_recursionDepth > orderbook::kMaxRecursionDepth) {
            state.m_recursionDepth = 0;  // Reset for recovery // Renamed to m_snake_case
            return false;
        }
        return true;
    }
    
    static void exitCriticalOperation() noexcept {
        if (isTestMode()) {
            return;
        }
        
        auto& state = getThreadState();
        if (state.m_recursionDepth > 0) { // Renamed to m_snake_case
            --state.m_recursionDepth; // Renamed to m_snake_case
        }
    }

    // RAII Guard for critical operations
    class CriticalGuard {
    public:
        CriticalGuard() noexcept : m_isValid(enterCriticalOperation()) {} // Renamed to m_snake_case, camelCase
        
        ~CriticalGuard() noexcept {
            if (m_isValid) { // Renamed to m_snake_case
                exitCriticalOperation();
            }
        }
        
        // Non-copyable and non-movable
        CriticalGuard(const CriticalGuard&) = delete;
        CriticalGuard& operator=(const CriticalGuard&) = delete;
        CriticalGuard(CriticalGuard&&) = delete;
        CriticalGuard& operator=(CriticalGuard&&) = delete;
        
        [[nodiscard]] bool isValid() const noexcept { return m_isValid; }
        
    private:
        bool m_isValid; // Renamed to m_snake_case
    };

    // Circuit breaker implementation
    [[nodiscard]] static bool circuitBreakerAllow() noexcept {
        if (!s_safetyEnabled.load(std::memory_order_relaxed)) {
            return true;
        }
        
        const int failures = s_failureCount.load(std::memory_order_relaxed);
        if (failures > kFailureThreshold) { // Renamed to kPascalCase
            const auto lastFailure = s_lastFailureTime.load(std::memory_order_relaxed);
            const auto now = std::chrono::steady_clock::now();
            
            if (now - lastFailure > kCooldownPeriod) { // Renamed to kPascalCase
                s_failureCount.store(0, std::memory_order_relaxed);
                return true;
            }
            return false;  // Circuit is open
        }
        return true;
    }
    
    static void recordFailure() noexcept {
        s_failureCount.fetch_add(1, std::memory_order_relaxed);
        s_lastFailureTime.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
    }
    
    static void recordSuccess() noexcept {
        int expected = s_failureCount.load(std::memory_order_relaxed);
        while (expected > 0) {
            if (s_failureCount.compare_exchange_weak(
                    expected, expected - 1,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }
    }
    
    // Modern utility methods
    [[nodiscard]] static int getFailureCount() noexcept {
        return s_failureCount.load(std::memory_order_relaxed);
    }
    
    [[nodiscard]] static bool isEnabled() noexcept {
        return s_safetyEnabled.load(std::memory_order_relaxed);
    }
    
    [[nodiscard]] static TimePoint getLastFailureTime() noexcept {
        return s_lastFailureTime.load(std::memory_order_relaxed);
    }
    
    // Reset circuit breaker manually
    static void resetCircuitBreaker() noexcept {
        s_failureCount.store(0, std::memory_order_relaxed);
        s_lastFailureTime.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
    }
};

// Modern macros with safer behavior
#define PRODUCTION_CRITICAL_GUARD \
    ProductionSafety::CriticalGuard guardLocal; \
    if (!guardLocal.isValid()) { \
        ProductionSafety::recordFailure(); \
        return; /* graceful early return */ \
    }

#define PRODUCTION_CIRCUIT_BREAKER \
    if (!ProductionSafety::circuitBreakerAllow()) { \
        ProductionSafety::recordFailure(); \
        return; /* graceful early return */ \
    }

// C++20 concepts for safety-enabled types
template<typename T>
concept SafetyAware = requires(T t) {
    { t.isValid() } -> std::convertible_to<bool>;
};

// Modern utility functions
namespace ProductionSafetyUtils {
    [[nodiscard]] inline bool checkAndRecord(bool condition) noexcept {
        if (condition) {
            ProductionSafety::recordSuccess();
            return true;
        } else {
            ProductionSafety::recordFailure();
            return false;
        }
    }
    
    template<typename Func>
    requires std::invocable<Func>
    [[nodiscard]] inline auto safeExecute(Func&& func) noexcept
        -> std::invoke_result_t<Func> {
        using ReturnType = std::invoke_result_t<Func>;
        
        if constexpr (std::is_void_v<ReturnType>) {
            PRODUCTION_CRITICAL_GUARD
            std::forward<Func>(func)();
        } else {
            PRODUCTION_CRITICAL_GUARD
            return std::forward<Func>(func)();
        }
    }
}
