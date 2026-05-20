#pragma once

/**
 * Insert Result Types - Modern C++20 Error Handling
 * 
 * Provides expected-based result types with configurable
 * error handling strategies for OrderBook::insertOrder operations.
 */

#include <atomic>     // For std::atomic
#include <chrono>     // For std::chrono
#include <expected>   // For std::expected
#include <format>     // For std::format
#include <functional>
#include <iostream>   // For std::cerr
#include <optional>   // For std::optional
#include <sstream>    // For std::stringstream
#include <source_location>
#include <stdexcept>  // For std::runtime_error
#include <string>     // For std::string
#include <string_view> // For std::string_view
#include <thread>
#include <variant>    // For std::variant
#include "semantic_types.h"
#include "constants.h"

namespace orderbook {

// Result status codes
enum class InsertError {
    Success = 0, // Operation completed successfully
    NullOrder, // Provided order pointer was null
    InvalidQuantity, // Order quantity was invalid (e.g., <= 0)
    RecursionLimitExceeded, // Stack recursion limit was exceeded
    StackOverflowProtection, // Stack overflow protection triggered
    CircuitBreakerOpen, // Circuit breaker is open, preventing operations
    LockAcquisitionFailed, // Failed to acquire a necessary lock
    OrderAlreadyExists, // An order with the same ID already exists
    InternalError // Generic internal error
};

// Error context with source location (C++20)
struct ErrorContext {
    InsertError code;
    std::string message;
    std::source_location location;
    std::chrono::steady_clock::time_point timestamp;
    int recursion_depth{0};
    std::thread::id thread_id;
    
    explicit ErrorContext(
        InsertError err,
        std::string_view msg = "",
        std::source_location loc = std::source_location::current()
    ) : code(err),
        message(msg),
        location(loc),
        timestamp(std::chrono::steady_clock::now()),
        thread_id(std::this_thread::get_id())
    {}
    
    std::string toString() const {
        return std::format("[{}] InsertError::{} at {}:{} (depth: {}) - {}",
            std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count(),
            static_cast<int>(code),
            location.file_name(),
            location.line(),
            recursion_depth,
            message);
    }
};

/**
 * Helper struct to mimic std::unexpected
 */
template<typename E>
struct Unexpected {
    E error;
    explicit Unexpected(E e) : error(std::move(e)) {}
};

/**
 * Helper function to create an unexpected result
 */
template<typename E>
inline Unexpected<E> unexpected(E e) {
    return Unexpected<E>(std::move(e));
}

using OrderInsertResult = std::expected<ExchangeId, ErrorContext>;
using InsertResultContext = std::expected<void, ErrorContext>;

// ====================================================================
// Error Handling Strategies
// ====================================================================

enum class ErrorStrategy {
    LogAndContinue,      // Log error, return nullopt
    LogAndRetry,         // Log error, retry with backoff
    ThrowException,      // Throw runtime_error
    ReturnDefault,       // Return default value
    CustomHandler        // Use custom function
};

// Strategy configuration
struct StrategyConfig {
    ErrorStrategy primary_strategy{ErrorStrategy::LogAndContinue};
    int max_retries{3};
    std::chrono::milliseconds retry_delay{10};
    std::function<void(const ErrorContext&)> custom_handler{nullptr};
    bool enable_circuit_breaker{true};
    int circuit_breaker_threshold{10};
    std::chrono::seconds circuit_breaker_cooldown{30};
};

// C++23: Modern error handler with strategy pattern
class InsertErrorHandler {
public:
    using HandlerFunc = std::function<void(const ErrorContext&)>;
    using RetryFunc = std::function<OrderInsertResult(void)>;

private:
    StrategyConfig m_config; // Configuration for error handling strategies
    mutable std::atomic<int> m_failureCount{0}; // Atomic counter for consecutive failures
    mutable std::atomic<std::chrono::steady_clock::time_point> m_lastFailure; // Timestamp of the last recorded failure
    HandlerFunc logger_; // Custom logger function

public:
    explicit InsertErrorHandler(StrategyConfig config = {})
        : m_config(std::move(config)),
          m_lastFailure(std::chrono::steady_clock::now())
    {
        // Default logger using std::format
        logger_ = [](const ErrorContext& ctx) {
            std::cerr << std::format("[ORDERBOOK ERROR] {}\n", ctx.toString());
        };
    }

    // C++23: Explicitly delete copy to enforce move semantics
    InsertErrorHandler(const InsertErrorHandler&) = delete; // std::atomic is not copyable
    InsertErrorHandler& operator=(const InsertErrorHandler&) = delete; // std::atomic is not copyable
    
    InsertErrorHandler(InsertErrorHandler&&) = delete; // std::atomic is not movable
    InsertErrorHandler& operator=(InsertErrorHandler&&) = delete; // std::atomic is not movable

    // Set custom logger
    void setLogger(HandlerFunc logger) {
        logger_ = std::move(logger);
    }

    // Set custom handler
    void setCustomHandler(HandlerFunc handler) {
        m_config.custom_handler = std::move(handler);
    }

    // Main handle method
    template<typename T>
    std::expected<T, ErrorContext> handle(const ErrorContext& ctx) const {
        // Always log first
        if (logger_) {
            logger_(ctx);
        }
        // Record failure for circuit breaker
        if (m_config.enable_circuit_breaker) {
            m_failureCount.fetch_add(1, std::memory_order_seq_cst);
            m_lastFailure.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        }

        // Execute strategy
        switch (m_config.primary_strategy) {
            case ErrorStrategy::LogAndContinue:
                return std::unexpected(ctx);
                
            case ErrorStrategy::LogAndRetry:
                return std::unexpected(ctx); // Caller must handle retry
                
            case ErrorStrategy::ThrowException:
                throw std::runtime_error(ctx.toString());
                
            case ErrorStrategy::ReturnDefault:
                return T{}; // Return default-constructed value
                
            case ErrorStrategy::CustomHandler:
                if (m_config.custom_handler) {
                    m_config.custom_handler(ctx);
                }
                return std::unexpected(ctx);
        }

        return std::unexpected(ctx);
    }

    // C++20: Retry logic with backoff
    template<typename T, typename Func>
    std::expected<T, ErrorContext> executeWithRetry(Func&& operation) {
        std::optional<std::expected<T, ErrorContext>> last_result;
        for (int attempt = 0; attempt <= m_config.max_retries; ++attempt) {
            auto result = operation();
            // result is declared here, so it's scope is within the loop.
            
            if (result.has_value()) {
                // Record success - decrement failure count
                decrementFailureCount();
                return result;
            }
            last_result = result;
            
            if (attempt < m_config.max_retries) {
                // Exponential backoff
                auto delay = m_config.retry_delay * (1 << attempt);
                std::this_thread::sleep_for(delay);
            }
        }
        
        // All retries exhausted - return the last error result
        return *last_result;
    }

    // Circuit breaker check
    [[nodiscard]] bool isCircuitBreakerOpen() const {
        if (!m_config.enable_circuit_breaker) {
            return false;
        }
        
        const int failures = m_failureCount.load(std::memory_order_relaxed);
        if (failures < m_config.circuit_breaker_threshold) {
            return false;
        }
        
        const auto last = m_lastFailure.load(std::memory_order_relaxed);
        const auto now = std::chrono::steady_clock::now();
        
        // Check if cooldown period has passed
        if (now - last > m_config.circuit_breaker_cooldown) {
            // Reset circuit breaker
            m_failureCount.store(0, std::memory_order_relaxed);
            return false;
        }
        
        return true;
    }

private:
    void decrementFailureCount() const {
        int expected = m_failureCount.load(std::memory_order_relaxed);
        while (expected > 0) {
            if (m_failureCount.compare_exchange_weak( // Renamed to m_snake_case
                    expected, expected - 1,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }
    }
};

// ====================================================================
// Stack Overflow Protection
// ====================================================================

class StackProtection {
public:
    // Configuration constants from constants.h
    // MAX_RECURSION_DEPTH = 100 (or as defined in constants.h)
    // WARNING_THRESHOLD = 80 (or as defined in constants.h)

private:
    struct ThreadState {
        int m_depth{0};
        std::chrono::steady_clock::time_point m_lastResetTime{std::chrono::steady_clock::now()};
    };

public:
    [[nodiscard]] static bool enterOperation() noexcept {
        auto& state = getThreadState();
        
        const auto now = std::chrono::steady_clock::now();
        if (now - state.m_lastResetTime > kResetInterval) {
            state.m_depth = 0;
            state.m_lastResetTime = now;
        }
        
        if (++state.m_depth > orderbook::kMaxRecursionDepth) { // Renamed to kPascalCase
            --state.m_depth;
            return false;
        }
        
        return true;
    }

    static void exitOperation() noexcept {
        auto& state = getThreadState();
        if (state.m_depth > 0) {
            --state.m_depth;
        }
    }

    [[nodiscard]] static int currentDepth() noexcept { // Renamed to camelCase
        return getThreadState().m_depth;
    }

    [[nodiscard]] static bool isNearLimit() noexcept { // Renamed to camelCase
        return getThreadState().m_depth >= orderbook::kWarningThreshold;
    }

private:
    [[nodiscard]] static ThreadState& getThreadState() noexcept {
        thread_local ThreadState state;
        return state;
    }
};

// RAII guard for automatic stack protection
class StackGuard {
public:
    explicit StackGuard(bool& successFlag) noexcept
        : m_success(StackProtection::enterOperation()),
          m_flagRef(successFlag) {
        m_flagRef = m_success;
    }
    // Destructor
    ~StackGuard() noexcept {
        if (m_success) { // Renamed to m_snake_case
            StackProtection::exitOperation();
        }
    }

    StackGuard(const StackGuard&) = delete;
    StackGuard& operator=(const StackGuard&) = delete;
    StackGuard(StackGuard&&) = delete;
    StackGuard& operator=(StackGuard&&) = delete;

    [[nodiscard]] bool isValid() const noexcept { return m_success; }

private:
    bool m_success;
    bool& m_flagRef;
};

// ====================================================================
// Result Utilities
// ====================================================================

// Helper to convert legacy result to modern result
inline OrderInsertResult legacyToModern(std::optional<ExchangeId> legacyResult,
                                         std::source_location loc = std::source_location::current()) {
    if (legacyResult.has_value()) {
        return OrderInsertResult(*legacyResult);
    }
    return std::unexpected(ErrorContext(InsertError::InternalError, orderbook::EngineConstants::kLegacyNulloptResult, loc));
}

} // namespace orderbook

// Using declarations for convenience
using orderbook::OrderInsertResult;
using orderbook::InsertError;
using orderbook::ErrorContext;
using orderbook::InsertErrorHandler;
using orderbook::StrategyConfig;
using orderbook::ErrorStrategy; // Renamed to camelCase
using orderbook::StackProtection;
using orderbook::StackGuard;
using orderbook::legacyToModern;
using orderbook::unexpected;
