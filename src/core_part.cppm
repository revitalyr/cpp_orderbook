module;

#include <atomic>
#include <array>
#include <boost/intrusive/list_hook.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/link_mode.hpp>

#include <cfloat>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#if defined(__aarch64__)
#else
#include <emmintrin.h>
#include <immintrin.h>
#include <xmmintrin.h>
#endif

#include "fixed.h"

export module orderbook:core;

import orderbook.semantic_types;
import orderbook.constants;

// ============================================================================
// ProductionSafety — at global scope for compatibility with headers
// ============================================================================

class ProductionSafety {
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::steady_clock::duration;
    static constexpr int kMaxRecursionDepth = 100;
    static constexpr int kWarningThreshold = 70;
    static constexpr Duration kResetInterval{std::chrono::seconds{1}};
    static constexpr Duration kCooldownPeriod{std::chrono::seconds{30}};
    static constexpr int kFailureThreshold = 10;
private:
    struct ThreadLocalState {
        int m_recursionDepth{0};
        TimePoint m_lastResetTime{std::chrono::steady_clock::now()};
    };
    static inline std::atomic<bool> s_safetyEnabled{true};
    static inline std::atomic<int> s_failureCount{0};
    static inline std::atomic<TimePoint> s_lastFailureTime{std::chrono::steady_clock::now()};
    static ThreadLocalState& getThreadState() noexcept {
        thread_local ThreadLocalState state;
        return state;
    }
public:
    static void enableSafety(bool enabled = true) noexcept { s_safetyEnabled.store(enabled, std::memory_order_relaxed); }
    static bool isTestMode() noexcept { return !s_safetyEnabled.load(std::memory_order_relaxed); }
    static bool enterCriticalOperation() noexcept {
        if (isTestMode()) return true;
        auto& state = getThreadState();
        const auto now = std::chrono::steady_clock::now();
        if (now - state.m_lastResetTime > kResetInterval) {
            state.m_recursionDepth = 0;
            state.m_lastResetTime = now;
        }
        if (++state.m_recursionDepth > kMaxRecursionDepth) {
            state.m_recursionDepth = 0;
            return false;
        }
        return true;
    }
    static void exitCriticalOperation() noexcept {
        if (isTestMode()) return;
        auto& state = getThreadState();
        if (state.m_recursionDepth > 0) --state.m_recursionDepth;
    }
    class CriticalGuard {
    public:
        CriticalGuard() noexcept : m_isValid(enterCriticalOperation()) {}
        ~CriticalGuard() noexcept { if (m_isValid) exitCriticalOperation(); }
        CriticalGuard(const CriticalGuard&) = delete;
        CriticalGuard& operator=(const CriticalGuard&) = delete;
        CriticalGuard(CriticalGuard&&) = delete;
        CriticalGuard& operator=(CriticalGuard&&) = delete;
        bool isValid() const noexcept { return m_isValid; }
    private:
        bool m_isValid;
    };
    static bool circuitBreakerAllow() noexcept {
        if (!s_safetyEnabled.load(std::memory_order_relaxed)) return true;
        const int failures = s_failureCount.load(std::memory_order_relaxed);
        if (failures > kFailureThreshold) {
            const auto lastFailure = s_lastFailureTime.load(std::memory_order_relaxed);
            const auto now = std::chrono::steady_clock::now();
            if (now - lastFailure > kCooldownPeriod) {
                s_failureCount.store(0, std::memory_order_relaxed);
                return true;
            }
            return false;
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
            if (s_failureCount.compare_exchange_weak(expected, expected - 1, std::memory_order_relaxed, std::memory_order_relaxed)) break;
        }
    }
    static int getFailureCount() noexcept { return s_failureCount.load(std::memory_order_relaxed); }
    static bool isEnabled() noexcept { return s_safetyEnabled.load(std::memory_order_relaxed); }
    static TimePoint getLastFailureTime() noexcept { return s_lastFailureTime.load(std::memory_order_relaxed); }
    static void resetCircuitBreaker() noexcept {
        s_failureCount.store(0, std::memory_order_relaxed);
        s_lastFailureTime.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
    }
};

export namespace orderbook {

// ============================================================================
// SpinLock
// ============================================================================

class SpinLock {
    std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;
public:
    void lock() {
        while (true) {
            while (m_mutex.test(std::memory_order_acquire)) {
#if defined(__aarch64__)
                asm volatile("yield" ::: "memory");
#else
                _mm_pause();
#endif
            }
            if (!m_mutex.test_and_set(std::memory_order_acquire)) {
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

// ============================================================================
// StringInterner
// ============================================================================

class StringInterner {
public:
    using StringId = uint32_t;
    static constexpr StringId INVALID_ID = 0;
private:
    struct StringData {
        std::unique_ptr<char[]> data;
        size_t length;
        size_t hash;
    };
    std::vector<StringData> m_strings;
    std::unordered_map<size_t, std::vector<StringId>> m_hashToIds;
    mutable std::shared_mutex m_mutex;
    static size_t compute_hash(std::string_view sv) {
        size_t hash = 14695981039346656037ull;
        for (char c : sv) {
            hash ^= static_cast<size_t>(static_cast<unsigned char>(c));
            hash *= 1099511628211ull;
        }
        return hash;
    }
public:
    StringInterner() {
        m_strings.reserve(kInitialStringInternerCapacity);
        m_strings.push_back({nullptr, 0, 0});
    }
    StringId intern(std::string_view sv) {
        if (sv.empty()) return INVALID_ID;
        size_t hash = compute_hash(sv);
        thread_local std::unordered_map<size_t, StringId> tl_cache;
        auto cache_it = tl_cache.find(hash);
        if (cache_it != tl_cache.end()) return cache_it->second;
        {
            std::shared_lock lock(m_mutex);
            auto it = m_hashToIds.find(hash);
            if (it != m_hashToIds.end()) {
                for (StringId id : it->second) {
                    const auto& str = m_strings[id];
                    if (str.length == sv.length() &&
                        std::memcmp(str.data.get(), sv.data(), sv.length()) == 0) {
                        tl_cache[hash] = id;
                        return id;
                    }
                }
            }
        }
        std::unique_lock lock(m_mutex);
        auto it = m_hashToIds.find(hash);
        if (it != m_hashToIds.end()) {
            for (StringId id : it->second) {
                const auto& str = m_strings[id];
                if (str.length == sv.length() &&
                    std::memcmp(str.data.get(), sv.data(), sv.length()) == 0) {
                    return id;
                }
            }
        }
        StringId new_id = static_cast<StringId>(m_strings.size());
        auto buffer = std::make_unique<char[]>(sv.length() + 1);
        std::memcpy(buffer.get(), sv.data(), sv.length());
        buffer[sv.length()] = '\0';
        m_strings.push_back({std::move(buffer), sv.length(), hash});
        m_hashToIds[hash].push_back(new_id);
        tl_cache[hash] = new_id;
        return new_id;
    }
    std::string_view get(StringId id) const {
        if (id == INVALID_ID || id >= m_strings.size()) return {};
        std::shared_lock lock(m_mutex);
        const auto& str = m_strings[id];
        return std::string_view(str.data.get(), str.length);
    }
    size_t size() const {
        std::shared_lock lock(m_mutex);
        return m_strings.size() - 1;
    }
    void reserve(size_t n) {
        std::unique_lock lock(m_mutex);
        m_strings.reserve(n + 1);
    }
};

inline StringInterner& g_globalStringInterner() {
    static StringInterner interner;
    return interner;
}

// ============================================================================
// MemoryPool
// ============================================================================

template<typename T, size_t BlockSize = kMemoryPoolBlockSize>
class MemoryPool {
public:
    static constexpr size_t ActualNodeSize = (sizeof(T) > kNodeSize) ? sizeof(T) : kNodeSize;
    struct PoolNode {
        alignas(64) char object_storage[ActualNodeSize];
        std::atomic<PoolNode*> next{nullptr};
        T* object() { return reinterpret_cast<T*>(object_storage); }
    };
private:
    struct alignas(64) Block {
        char m_data[BlockSize * sizeof(PoolNode)];
        Block* m_next{nullptr};
    };
    mutable SpinLock m_lock;
    std::vector<std::unique_ptr<Block>> m_blocks;
    PoolNode* m_freeList{nullptr};
    std::atomic<size_t> m_allocatedCount{0};
    std::atomic<size_t> m_capacity{0};
public:
    MemoryPool() = default;
    ~MemoryPool() = default;
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    void reserve(size_t n) {
        std::lock_guard lock(m_lock);
        size_t current = m_capacity.load(std::memory_order_relaxed);
        if (n <= current) return;
        size_t blocks_needed = (n - current + BlockSize - 1) / BlockSize;
        for (size_t b = 0; b < blocks_needed; ++b) {
            auto block = std::make_unique<Block>();
            PoolNode* nodes = reinterpret_cast<PoolNode*>(block->m_data);
            for (size_t i = 0; i < BlockSize - 1; ++i) {
                nodes[i].next.store(&nodes[i + 1], std::memory_order_relaxed);
            }
            nodes[BlockSize - 1].next.store(m_freeList, std::memory_order_relaxed);
            m_freeList = &nodes[0];
            m_blocks.push_back(std::move(block));
        }
        m_capacity.fetch_add(blocks_needed * BlockSize, std::memory_order_relaxed);
    }
    T* allocate() {
        PoolNode* node = popFree();
        if (!node) return nullptr;
        m_allocatedCount.fetch_add(1, std::memory_order_relaxed);
        return reinterpret_cast<T*>(node);
    }
    void deallocate(T* ptr) {
        if (!ptr) return;
        ptr->~T();
        PoolNode* node = reinterpret_cast<PoolNode*>(ptr);
        pushFree(node);
        m_allocatedCount.fetch_sub(1, std::memory_order_relaxed);
    }
    template <typename... Args>
    T* construct(Args&&... args) {
        T* ptr = allocate();
        if (ptr) {
            new (ptr) T(std::forward<Args>(args)...);
        }
        return ptr;
    }
    size_t allocated_count() const { return m_allocatedCount.load(std::memory_order_relaxed); }
    size_t capacity() const { return m_capacity.load(std::memory_order_relaxed); }
    PoolNode* popFree() {
        std::lock_guard lock(m_lock);
        if (!m_freeList) return nullptr;
        PoolNode* node = m_freeList;
        m_freeList = node->next.load(std::memory_order_relaxed);
        return node;
    }
    void pushFree(PoolNode* node) {
        std::lock_guard lock(m_lock);
        node->next.store(m_freeList, std::memory_order_relaxed);
        m_freeList = node;
    }
};

template <typename T, typename PoolType>
struct MemoryPoolAllocator {
    using value_type = T;
    MemoryPoolAllocator() = default;
    template <typename U>
    MemoryPoolAllocator(const MemoryPoolAllocator<U, PoolType>&) noexcept {}
    T* allocate(std::size_t n) {
        if (n != 1) throw std::bad_array_new_length();
        return reinterpret_cast<T*>(PoolType::instance().popFree());
    }
    void deallocate(T* p, std::size_t) noexcept {
        auto& pool = PoolType::instance();
        pool.pushFree(reinterpret_cast<typename std::remove_reference_t<decltype(pool)>::PoolNode*>(p));
    }
    bool operator==(const MemoryPoolAllocator&) const = default;
};

// ============================================================================
// ProductionSafety alias
// ============================================================================

// ProductionSafety is defined at file scope above

// ============================================================================
// Order
// ============================================================================

inline Timestamp epoch() {
    return std::chrono::system_clock::now();
}

using StringId = StringInterner::StringId;

class OrderPool;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

struct alignas(64) Order {
    enum class Side : uint8_t { BUY = 0, SELL = 1 };
    template <typename TListener> friend class OrderBook;
    friend class OrderList;
    friend class OrderMap;
    template <typename TListener> friend class Exchange;
    template<typename> friend class PointerPriceLevels;
    template<typename> friend class StructPriceLevels;
    template<typename> friend class MapPriceLevels;
    template<typename> friend class MapPtrPriceLevels;
    static std::shared_ptr<Order> create(
        SessionIdView sessionId,
        OrderIdStrView orderId,
        InstrumentSymbolView instrument,
        Price price,
        Quantity quantity,
        Order::Side side,
        ExchangeId exchangeId
    );
private:
    boost::intrusive::list_member_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> m_listHook;
    bool m_onList = false;
    std::atomic<Order*> m_nextPtr{nullptr};
    const Timestamp m_timeSubmitted;
    ExchangeId m_orderIdNum = ExchangeId(0);
    Price m_price;
    Price m_averagePrice = Price(0);
    Quantity m_remaining{0};
    Quantity m_filled{0};
    Quantity m_quantity{0};
    Quantity m_cumulativeQuantity{0};
    StringId m_sessionId = StringInterner::INVALID_ID;
    StringId m_orderId = StringInterner::INVALID_ID;
    StringId m_instrumentId = StringInterner::INVALID_ID;
public:
    const ExchangeId m_exchangeId;
    const Side m_side;
    bool m_isQuote = false;
private:
    void fill(Quantity quantity, Price price) noexcept {
        m_remaining -= quantity;
        m_filled += quantity;
        m_averagePrice = (m_averagePrice * m_cumulativeQuantity + price * quantity) / (m_cumulativeQuantity + quantity);
        m_cumulativeQuantity += quantity;
    }
    void cancel() noexcept { m_remaining = Quantity(0); }
    bool isMarket() const noexcept {
        return m_price == kMarketBuyPrice || m_price == kMarketSellPrice;
    }
public:
    std::string sessionId() const {
        return std::string(g_globalStringInterner().get(m_sessionId));
    }
    std::string orderId() const {
        if (m_orderId != StringInterner::INVALID_ID) {
            return std::string(g_globalStringInterner().get(m_orderId));
        }
        return std::to_string(m_orderIdNum);
    }
    std::string instrument() const {
        return std::string(g_globalStringInterner().get(m_instrumentId));
    }
    ExchangeId orderIdNum() const noexcept { return m_orderIdNum; }
    Price price() const noexcept { return m_price; }
    Quantity quantity() const noexcept { return m_quantity; }
    bool isOnList() const noexcept { return m_onList; }
    Quantity remainingQuantity() const noexcept { return m_remaining; }
    Quantity filledQuantity() const noexcept { return m_filled; }
    Quantity cumulativeQuantity() const noexcept { return m_cumulativeQuantity; }
    Price averagePrice() const noexcept { return m_averagePrice; }
    bool isCancelled() const noexcept { return m_remaining == Quantity(0) && m_filled != m_quantity; }
    bool isFilled() const noexcept { return m_remaining == Quantity(0) && m_filled == m_quantity; }
    bool isPartiallyFilled() const noexcept { return m_remaining == Quantity(0) && m_filled > Quantity(0); }
    bool isActive() const noexcept { return m_remaining > 0; }
    Order(const Order& other)
        : m_listHook(), m_onList(false), m_nextPtr(nullptr),
          m_timeSubmitted(other.m_timeSubmitted), m_orderIdNum(other.m_orderIdNum),
          m_price(other.m_price), m_averagePrice(other.m_averagePrice),
          m_remaining(other.m_remaining), m_filled(other.m_filled),
          m_quantity(other.m_quantity), m_cumulativeQuantity(other.m_cumulativeQuantity),
          m_sessionId(other.m_sessionId), m_orderId(other.m_orderId),
          m_instrumentId(other.m_instrumentId), m_exchangeId(other.m_exchangeId),
          m_side(other.m_side), m_isQuote(other.m_isQuote) {}
    Order(SessionIdView sessionId, OrderIdStrView orderId,
          InstrumentSymbolView instrument, Price price, Quantity quantity,
          Order::Side side, ExchangeId exchangeId)
        : m_listHook(), m_onList(false),
          m_timeSubmitted(epoch()), m_price(price), m_remaining(quantity),
          m_quantity(quantity),
          m_sessionId(g_globalStringInterner().intern(sessionId)),
          m_instrumentId(g_globalStringInterner().intern(instrument)),
          m_exchangeId(exchangeId), m_side(side) {
        if (!orderId.empty() && std::isdigit(static_cast<unsigned char>(orderId[0]))) {
            auto [ptr, ec] = std::from_chars(orderId.data(), orderId.data() + orderId.size(), m_orderIdNum);
            if (ec != std::errc()) {
                m_orderIdNum = 0;
                m_orderId = g_globalStringInterner().intern(orderId);
            }
        } else if (!orderId.empty()) {
            m_orderId = g_globalStringInterner().intern(orderId);
            m_orderIdNum = 0;
        }
    }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

class OrderPool {
public:
    static MemoryPool<Order>& instance() {
        static MemoryPool<Order> pool;
        return pool;
    }
    static void reserve(size_t n) { instance().reserve(n); }
};

inline std::shared_ptr<Order> Order::create(
    SessionIdView sessionId,
    OrderIdStrView orderId,
    InstrumentSymbolView instrument,
    Price price,
    Quantity quantity,
    Order::Side side,
    ExchangeId exchangeId
) {
    using Allocator = MemoryPoolAllocator<Order, OrderPool>;
    return std::allocate_shared<Order>(Allocator{},
        sessionId, orderId, instrument, price, quantity, side, exchangeId);
}

// ============================================================================
// OrderList
// ============================================================================

namespace bi = boost::intrusive;

class OrderList {
    template <typename TListener> friend class OrderBook;
private:
    using Hook = bi::list_member_hook<bi::link_mode<bi::normal_link>>;
    using MemberHook = bi::member_hook<Order, Hook, &Order::m_listHook>;
    using List = bi::list<Order, MemberHook, bi::constant_time_size<false>>;

    List m_list;
    Price m_price;
    Quantity m_totalQuantity{0};
public:
    OrderList(Price price) : m_price(price) {}
    OrderList(OrderList&&) noexcept = default;
    OrderList& operator=(OrderList&&) noexcept = default;
    OrderList(const OrderList&) = delete;
    OrderList& operator=(const OrderList&) = delete;
    ~OrderList() = default;
    const Price& price() const { return m_price; }
    Quantity totalQuantity() const noexcept { return m_totalQuantity; }

    void pushBack(Order& order) {
        order.m_onList = true;
        m_totalQuantity += order.remainingQuantity();
        m_list.push_back(order);
    }

    void remove(Order& order) {
        if (!order.m_onList)
            throw std::runtime_error("node is null on removal");
        order.m_onList = false;
        m_totalQuantity -= order.remainingQuantity();
        m_list.erase(List::s_iterator_to(order));
    }

    Order* front() {
        return m_list.empty() ? nullptr : &m_list.front();
    }

    const Order* front() const {
        return m_list.empty() ? nullptr : &m_list.front();
    }

    using Iterator = List::const_iterator;
    Iterator begin() const { return m_list.begin(); }
    Iterator end() const { return m_list.end(); }
};

// ============================================================================
// Insert Result Types
// ============================================================================

enum class InsertError {
    Success = 0,
    NullOrder,
    InvalidQuantity,
    RecursionLimitExceeded,
    StackOverflowProtection,
    CircuitBreakerOpen,
    LockAcquisitionFailed,
    OrderAlreadyExists,
    InternalError
};

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
    ) : code(err), message(msg), location(loc),
        timestamp(std::chrono::steady_clock::now()),
        thread_id(std::this_thread::get_id()) {}
    std::string toString() const {
        return std::format("[{}] InsertError::{} at {}:{} (depth: {}) - {}",
            std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count(),
            static_cast<int>(code), location.file_name(), location.line(),
            recursion_depth, message);
    }
};

template<typename E>
struct Unexpected {
    E error;
    explicit Unexpected(E e) : error(std::move(e)) {}
};

template<typename E>
inline Unexpected<E> unexpected(E e) {
    return Unexpected<E>(std::move(e));
}

using OrderInsertResult = std::expected<ExchangeId, ErrorContext>;
using InsertResultContext = std::expected<void, ErrorContext>;

enum class ErrorStrategy {
    LogAndContinue,
    LogAndRetry,
    ThrowException,
    ReturnDefault,
    CustomHandler
};

struct StrategyConfig {
    ErrorStrategy primary_strategy{ErrorStrategy::LogAndContinue};
    int max_retries{3};
    std::chrono::milliseconds retry_delay{10};
    std::function<void(const ErrorContext&)> custom_handler{nullptr};
    bool enable_circuit_breaker{true};
    int circuit_breaker_threshold{10};
    std::chrono::seconds circuit_breaker_cooldown{30};
};

class InsertErrorHandler {
public:
    using HandlerFunc = std::function<void(const ErrorContext&)>;
    using RetryFunc = std::function<OrderInsertResult(void)>;
private:
    StrategyConfig m_config;
    mutable std::atomic<int> m_failureCount{0};
    mutable std::atomic<std::chrono::steady_clock::time_point> m_lastFailure;
    HandlerFunc logger_;
public:
    explicit InsertErrorHandler(StrategyConfig config = {})
        : m_config(std::move(config)),
          m_lastFailure(std::chrono::steady_clock::now()) {
        logger_ = [](const ErrorContext& ctx) {
            std::cerr << std::format("[ORDERBOOK ERROR] {}\n", ctx.toString());
        };
    }
    InsertErrorHandler(const InsertErrorHandler&) = delete;
    InsertErrorHandler& operator=(const InsertErrorHandler&) = delete;
    InsertErrorHandler(InsertErrorHandler&&) = delete;
    InsertErrorHandler& operator=(InsertErrorHandler&&) = delete;
    void setLogger(HandlerFunc logger) { logger_ = std::move(logger); }
    void setCustomHandler(HandlerFunc handler) { m_config.custom_handler = std::move(handler); }
    template<typename T>
    std::expected<T, ErrorContext> handle(const ErrorContext& ctx) const {
        if (logger_) logger_(ctx);
        if (m_config.enable_circuit_breaker) {
            m_failureCount.fetch_add(1, std::memory_order_seq_cst);
            m_lastFailure.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
        }
        switch (m_config.primary_strategy) {
            case ErrorStrategy::LogAndContinue: return std::unexpected(ctx);
            case ErrorStrategy::LogAndRetry: return std::unexpected(ctx);
            case ErrorStrategy::ThrowException: throw std::runtime_error(ctx.toString());
            case ErrorStrategy::ReturnDefault: return T{};
            case ErrorStrategy::CustomHandler:
                if (m_config.custom_handler) m_config.custom_handler(ctx);
                return std::unexpected(ctx);
        }
        return std::unexpected(ctx);
    }
    template<typename T, typename Func>
    std::expected<T, ErrorContext> executeWithRetry(Func&& operation) {
        std::optional<std::expected<T, ErrorContext>> last_result;
        for (int attempt = 0; attempt <= m_config.max_retries; ++attempt) {
            auto result = operation();
            if (result.has_value()) {
                decrementFailureCount();
                return result;
            }
            last_result = result;
            if (attempt < m_config.max_retries) {
                auto delay = m_config.retry_delay * (1 << attempt);
                std::this_thread::sleep_for(delay);
            }
        }
        return *last_result;
    }
    bool isCircuitBreakerOpen() const {
        if (!m_config.enable_circuit_breaker) return false;
        const int failures = m_failureCount.load(std::memory_order_relaxed);
        if (failures < m_config.circuit_breaker_threshold) return false;
        const auto last = m_lastFailure.load(std::memory_order_relaxed);
        const auto now = std::chrono::steady_clock::now();
        if (now - last > m_config.circuit_breaker_cooldown) {
            m_failureCount.store(0, std::memory_order_relaxed);
            return false;
        }
        return true;
    }
private:
    void decrementFailureCount() const {
        int expected = m_failureCount.load(std::memory_order_relaxed);
        while (expected > 0) {
            if (m_failureCount.compare_exchange_weak(
                    expected, expected - 1,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }
    }
};

class StackProtection {
public:
    struct ThreadState {
        int m_depth{0};
        std::chrono::steady_clock::time_point m_lastResetTime{std::chrono::steady_clock::now()};
    };
    static bool enterOperation() noexcept {
        auto& state = getThreadState();
        const auto now = std::chrono::steady_clock::now();
        if (now - state.m_lastResetTime > kResetInterval) {
            state.m_depth = 0;
            state.m_lastResetTime = now;
        }
        if (++state.m_depth > kMaxRecursionDepth) {
            --state.m_depth;
            return false;
        }
        return true;
    }
    static void exitOperation() noexcept {
        auto& state = getThreadState();
        if (state.m_depth > 0) --state.m_depth;
    }
    static int currentDepth() noexcept { return getThreadState().m_depth; }
    static bool isNearLimit() noexcept { return getThreadState().m_depth >= kWarningThreshold; }
private:
    static ThreadState& getThreadState() noexcept {
        thread_local ThreadState state;
        return state;
    }
};

class StackGuard {
public:
    explicit StackGuard(bool& successFlag) noexcept
        : m_success(StackProtection::enterOperation()), m_flagRef(successFlag) {
        m_flagRef = m_success;
    }
    ~StackGuard() noexcept {
        if (m_success) StackProtection::exitOperation();
    }
    StackGuard(const StackGuard&) = delete;
    StackGuard& operator=(const StackGuard&) = delete;
    StackGuard(StackGuard&&) = delete;
    StackGuard& operator=(StackGuard&&) = delete;
    bool isValid() const noexcept { return m_success; }
private:
    bool m_success;
    bool& m_flagRef;
};

// ============================================================================
// PriceLevels
// ============================================================================

struct price_compare {
    explicit price_compare(bool ascending) : m_ascending(ascending) {}
    template<class T, class U>
    inline bool operator()(const T& t, const U& u) const {
        if constexpr (requires { t.price(); }) {
            return (m_ascending) ? t.price() < u : t.price() > u;
        } else {
            return (m_ascending) ? t < u : t > u;
        }
    }
    const bool m_ascending;
};

template <typename ContainerOfPtr>
class PointerPriceLevels {
private:
    const price_compare m_cmpFn;
    ContainerOfPtr m_levels;
public:
    PointerPriceLevels(bool ascending) : m_cmpFn(ascending) {}
    void insertOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(), m_cmpFn);
        std::shared_ptr<OrderList> list;
        if (itr == m_levels.end() || (*itr)->price() != order.price()) {
            list = std::make_shared<OrderList>(order.price());
            m_levels.insert(itr, list);
        } else {
            list = *itr;
        }
        list->pushBack(order);
    }
    void removeOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(), m_cmpFn);
        if (itr == m_levels.end() || (*itr)->price() != order.price()) {
            throw std::runtime_error("price level for order does not exist");
        }
        auto list = *itr;
        list->remove(order);
        if (list->front() == nullptr) m_levels.erase(itr);
    }
    Order* front() const {
        auto itr = m_levels.begin();
        return itr == m_levels.end() ? nullptr : (*itr)->front();
    }
    size_t size() const { return m_levels.size(); }
    void forEach(std::function<void(const OrderList*)> fn) const {
        for (auto itr = m_levels.begin(); itr != m_levels.end(); itr++) fn(*itr);
    }
};

template <typename ContainerOfStruct>
class StructPriceLevels {
private:
    const price_compare m_cmpFn;
    ContainerOfStruct m_levels;
public:
    StructPriceLevels(bool ascending) : m_cmpFn(ascending) {}
    void insertOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(), m_cmpFn);
        if (itr == m_levels.end() || itr->price() != order.price()) {
            OrderList list(order.price());
            list.pushBack(order);
            m_levels.insert(itr, std::move(list));
        } else {
            itr->pushBack(order);
        }
    }
    void removeOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(), m_cmpFn);
        if (itr == m_levels.end() || itr->price() != order.price()) {
            throw std::runtime_error("price level for order does not exist");
        }
        itr->remove(order);
        if (itr->front() == nullptr) m_levels.erase(itr);
    }
    Order* front() const {
        auto itr = m_levels.begin();
        if (itr == m_levels.end()) return nullptr;
        return itr->front();
    }
    bool empty() const { return m_levels.empty(); }
    size_t size() const { return m_levels.size(); }
    const ContainerOfStruct& levels() const { return m_levels; }
    void forEach(std::function<void(const OrderList*)> fn) const {
        for (auto itr = m_levels.begin(); itr != m_levels.end(); itr++) fn(&(*itr));
    }
};

template <typename MapOfStruct>
class MapPriceLevels {
private:
    const price_compare m_cmpFn;
    MapOfStruct m_levels;
public:
    MapPriceLevels(bool ascending) : m_cmpFn(ascending), m_levels(m_cmpFn) {}
    void insertOrder(Order& order) {
        auto itr = m_levels.lower_bound(order.price());
        if (itr == m_levels.end() || itr->first != order.price()) {
            OrderList list(order.price());
            list.pushBack(order);
            m_levels.insert({list.price(), std::move(list)});
        } else {
            itr->second.pushBack(order);
        }
    }
    void removeOrder(Order& order) {
        auto itr = m_levels.lower_bound(order.price());
        if (itr == m_levels.end() || itr->first != order.price()) {
            throw std::runtime_error("price level for order does not exist");
        }
        itr->second.remove(order);
        if (itr->second.front() == nullptr) m_levels.erase(itr);
    }
    Order* front() const {
        auto itr = m_levels.begin();
        if (itr == m_levels.end()) return nullptr;
        return itr->second.front();
    }
    bool empty() const { return m_levels.empty(); }
    size_t size() const { return m_levels.size(); }
    void forEach(std::function<void(const OrderList*)> fn) const {
        for (auto itr = m_levels.begin(); itr != m_levels.end(); itr++) fn(&(itr->second));
    }
};

template <typename MapOfPtr>
class MapPtrPriceLevels {
private:
    const price_compare m_cmpFn;
    MapOfPtr m_levels;
public:
    MapPtrPriceLevels(bool ascending) : m_cmpFn(ascending), m_levels(m_cmpFn) {}
    void insertOrder(Order& order) {
        auto itr = m_levels.lower_bound(order.price());
        if (itr == m_levels.end() || itr->first != order.price()) {
            auto list = std::make_shared<OrderList>(order.price());
            list->pushBack(order);
            m_levels.insert({list->price(), list});
        } else {
            itr->second->pushBack(order);
        }
    }
    void removeOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(), m_cmpFn);
        if (itr == m_levels.end() || itr->first != order.price()) {
            throw std::runtime_error("price level for order does not exist");
        }
        itr->second->remove(order);
        if (itr->second->front() == nullptr) m_levels.erase(itr);
    }
    Order* front() const {
        auto itr = m_levels.begin();
        if (itr == m_levels.end()) return nullptr;
        return itr->second->front();
    }
    bool empty() const { return m_levels.empty(); }
    const MapOfPtr& levels() const { return m_levels; }
    size_t size() const { return m_levels.size(); }
    void forEach(std::function<void(const OrderList*)> fn) const {
        for (auto itr = m_levels.begin(); itr != m_levels.end(); itr++) fn(itr->second.get());
    }
};

class PooledPriceLevels {
public:
    struct Entry {
        Price m_price;
        OrderList* m_orders;

        const Price& price() const { return m_price; }
        Quantity totalQuantity() const { return m_orders->totalQuantity(); }
    };

private:
    std::vector<Entry> m_levels;
    price_compare m_cmpFn;

    static MemoryPool<OrderList>& pool() {
        static MemoryPool<OrderList> p;
        return p;
    }

public:
    PooledPriceLevels(bool ascending) : m_cmpFn(ascending) {
        pool().reserve(256);
    }

    void insertOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(),
            [this](const Entry& e, Price p) { return m_cmpFn(e.m_price, p); });

        if (itr == m_levels.end() || itr->m_price != order.price()) {
            auto* list = pool().construct(order.price());
            if (!list) {
                pool().reserve(256);
                list = pool().construct(order.price());
            }
            list->pushBack(order);
            m_levels.insert(itr, {order.price(), list});
        } else {
            itr->m_orders->pushBack(order);
        }
    }

    void removeOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(),
            [this](const Entry& e, Price p) { return m_cmpFn(e.m_price, p); });

        if (itr == m_levels.end() || itr->m_price != order.price()) {
            throw std::runtime_error("price level for order does not exist");
        }

        itr->m_orders->remove(order);
        if (itr->m_orders->front() == nullptr) {
            pool().deallocate(itr->m_orders);
            m_levels.erase(itr);
        }
    }

    Order* front() const {
        if (m_levels.empty()) return nullptr;
        return m_levels.front().m_orders->front();
    }

    bool empty() const { return m_levels.empty(); }
    size_t size() const { return m_levels.size(); }

    void forEach(std::function<void(const OrderList*)> fn) const {
        for (const auto& entry : m_levels) {
            fn(entry.m_orders);
        }
    }

    const std::vector<Entry>& levels() const { return m_levels; }
};

using DequeuePtrPriceLevels = PointerPriceLevels<std::deque<std::shared_ptr<OrderList>>>;
using VectorPointerPriceLevels = PointerPriceLevels<std::vector<std::shared_ptr<OrderList>>>;
using VectorPriceLevels = StructPriceLevels<std::vector<OrderList>>;
using StdMapPriceLevels = MapPriceLevels<std::map<Price, OrderList, price_compare>>;

using StdMapPointerPriceLevels = MapPtrPriceLevels<std::map<Price, std::shared_ptr<OrderList>, price_compare>>;

using PriceLevels = PooledPriceLevels;

// ============================================================================
// OrderMap
// ============================================================================

class OrderMap {
private:
    struct Shard {
        mutable SpinLock mutex;
        std::unordered_map<ExchangeId, std::shared_ptr<Order>> map;
    };
    std::array<Shard, kOrderMapShards> m_shards;
    Shard& getShard(ExchangeId id) { return m_shards[static_cast<size_t>(id) % kOrderMapShards]; }
    const Shard& getShard(ExchangeId id) const { return m_shards[static_cast<size_t>(id) % kOrderMapShards]; }
public:
    OrderMap() {
        for (auto& shard : m_shards) shard.map.reserve(kDefaultOrderMapCapacity / kOrderMapShards);
    }
    void add(std::shared_ptr<Order> order) {
        if (!order) return;
        auto& shard = getShard(order->m_exchangeId);
        std::lock_guard lock(shard.mutex);
        shard.map[order->m_exchangeId] = std::move(order);
    }
    std::shared_ptr<Order> get(ExchangeId exchangeId) const {
        auto& shard = getShard(exchangeId);
        std::lock_guard lock(shard.mutex);
        auto it = shard.map.find(exchangeId);
        return (it != shard.map.end()) ? it->second : nullptr;
    }
    void remove(ExchangeId exchangeId) {
        auto& shard = getShard(exchangeId);
        std::lock_guard lock(shard.mutex);
        shard.map.erase(exchangeId);
    }
    std::vector<std::shared_ptr<const Order>> all() const {
        std::vector<std::shared_ptr<const Order>> orders;
        for (const auto& shard : m_shards) {
            std::lock_guard lock(shard.mutex);
            orders.reserve(orders.size() + shard.map.size());
            for (const auto& [id, order] : shard.map) orders.push_back(order);
        }
        return orders;
    }
    std::vector<InstrumentSymbol> instruments() const {
        std::unordered_set<InstrumentSymbol> unique_instruments;
        for (const auto& shard : m_shards) {
            std::lock_guard lock(shard.mutex);
            for (const auto& [id, order] : shard.map) unique_instruments.insert(order->instrument());
        }
        return std::vector<InstrumentSymbol>(unique_instruments.begin(), unique_instruments.end());
    }
    size_t size() const {
        size_t total = 0;
        for (const auto& shard : m_shards) {
            std::lock_guard lock(shard.mutex);
            total += shard.map.size();
        }
        return total;
    }
    void clear() {
        for (auto& shard : m_shards) {
            std::lock_guard lock(shard.mutex);
            shard.map.clear();
        }
    }
    void reserve(size_t n) {
        size_t perShard = n / kOrderMapShards;
        for (auto& shard : m_shards) {
            std::lock_guard lock(shard.mutex);
            shard.map.reserve(perShard);
        }
    }
};

// ============================================================================
// OrderBook
// ============================================================================

struct ExchangeListener {
    virtual void onOrder(const Order&) {}
    virtual void onTrade(const struct Trade&) {}
};

template <typename T>
concept Listener = requires(T listener, const Order& order, const struct Trade& trade) {
    { listener.onOrder(order) } -> std::same_as<void>;
    { listener.onTrade(trade) } -> std::same_as<void>;
};

struct Trade {
    template <typename TListener> friend class OrderBook;
private:
    Trade(Price price, Quantity quantity, const Order& aggressor, const Order& opposite, NanosecondTimestamp ts)
        : m_price(price), m_quantity(quantity), m_aggressor(aggressor), m_opposite(opposite), execId(ts) {}
public:
    const Price m_price;
    const Quantity m_quantity;
    const Order& m_aggressor;
    const Order& m_opposite;
    const NanosecondTimestamp execId;
};

struct BookLevel {
    Price m_price;
    Quantity m_quantity;
};

struct Book {
    std::vector<BookLevel> m_bids;
    std::vector<ExchangeId> m_bidOrderIds;
    std::vector<BookLevel> m_asks;
    std::vector<ExchangeId> m_askOrderIds;
};

inline std::ostream& operator<<(std::ostream& os, const Book& book) {
    bool first = true;
    for (auto side : {book.m_asks, book.m_bids}) {
        for (auto level : side) os << level.m_price << " " << level.m_quantity << "\n";
        if (first) { os << "----------\n"; first = false; }
    }
    return os;
}

struct QuoteOrders {
    std::shared_ptr<Order> m_bid = nullptr;
    std::shared_ptr<Order> m_ask = nullptr;
};

struct SessionQuoteId {
    const std::string m_sessionId;
    const std::string m_quoteId;
    SessionQuoteId(const std::string& sessionId, const std::string_view& quoteId)
        : m_sessionId(sessionId), m_quoteId(quoteId) {}
    bool operator<(const SessionQuoteId& other) const {
        return m_sessionId < other.m_sessionId || (m_sessionId == other.m_sessionId && m_quoteId < other.m_quoteId);
    }
    bool operator==(const SessionQuoteId& other) const {
        return m_sessionId == other.m_sessionId && m_quoteId == other.m_quoteId;
    }
};

inline std::ostream& operator<<(std::ostream& os, const SessionQuoteId& id) {
    return os << "[" << id.m_sessionId << ":" << id.m_quoteId << "]";
}

template <typename TListener> class Exchange;

template <typename TListener>
class OrderBook {
private:
    SpinLock m_mu;
    PriceLevels m_bids = PriceLevels(false);
    PriceLevels m_asks = PriceLevels(true);
    TListener& m_listener;
    void matchOrders(Order::Side aggressorSide) {
        while (!m_bids.empty() && !m_asks.empty()) {
            auto bid = m_bids.front();
            auto ask = m_asks.front();
        if (bid->isMarket() || ask->isMarket() || bid->m_price >= ask->m_price) {
            Quantity qty = std::min(bid->m_remaining, ask->m_remaining);
            Price price = (aggressorSide == Order::Side::BUY) ? ask->m_price : bid->m_price;
            auto* aggressor = aggressorSide == Order::Side::BUY ? bid : ask;
            auto* opposite  = aggressorSide == Order::Side::BUY ? ask : bid;
            bid->fill(qty, price);
            ask->fill(qty, price);
            const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const Trade trade(price, qty, *aggressor, *opposite, now_ns);
            if (bid->m_remaining == 0) m_bids.removeOrder(*bid);
            if (ask->m_remaining == 0) m_asks.removeOrder(*ask);
            m_listener.onOrder(*bid);
            m_listener.onOrder(*ask);
            m_listener.onTrade(trade);
        } else {
            break;
        }

        auto ordersOnSide = aggressorSide == Order::Side::BUY ? &m_bids : &m_asks;
        if (!ordersOnSide->empty()) {
            auto* order = ordersOnSide->front();
            if (order && order->isMarket()) {
                order->cancel();
                ordersOnSide->removeOrder(*order);
                m_listener.onOrder(*order);
            }
        }
        }
        auto ordersOnSide = aggressorSide == Order::Side::BUY ? &m_bids : &m_asks;
        if (!ordersOnSide->empty()) {
            auto order = ordersOnSide->front();
            if (order && order->isMarket()) {
                order->cancel();
                ordersOnSide->removeOrder(order);
                m_listener.onOrder(*order);
            }
        }
    }
    std::map<SessionQuoteId, QuoteOrders> m_quotes;
public:
    const std::string m_instrument;
    OrderBook(const std::string& instrument, TListener& listener) : m_listener(listener), m_instrument(instrument) {}
    ~OrderBook() = default;

    OrderInsertResult insertOrder(
        std::shared_ptr<Order> order
    ) {
        return insertOrderWithLocation(order);
    }

    OrderInsertResult insertOrderWithLocation(
        std::shared_ptr<Order> order,
        std::source_location location = std::source_location::current()
    ) {
        ProductionSafety::CriticalGuard stack_guard;
        if (!stack_guard.isValid()) {
            ErrorContext error(InsertError::StackOverflowProtection, EngineConstants::kRecursionDepthExceeded, location);
            error.recursion_depth = StackProtection::currentDepth();
            return std::unexpected(error);
        }
        if (!order) {
            return std::unexpected(ErrorContext(InsertError::NullOrder, EngineConstants::kOrderPointerNull, location));
        }
        if (order->remainingQuantity() <= 0) {
            return std::unexpected(ErrorContext(InsertError::InvalidQuantity,
                std::format("{}{}", EngineConstants::kInvalidOrderQuantity, order->remainingQuantity()), location));
        }
        if (order->isOnList()) {
            return std::unexpected(ErrorContext(InsertError::OrderAlreadyExists,
                std::format("{} ID: {}", EngineConstants::kOrderAlreadyOnList, order->m_exchangeId), location));
        }
    auto orderList = order->m_side == Order::Side::BUY ? &m_bids : &m_asks;
    orderList->insertOrder(*order);
        m_listener.onOrder(*order);
        matchOrders(order->m_side);
        return order->m_exchangeId;
    }



    int cancelOrder(std::shared_ptr<Order> order) {
        if (!order) return -1;
        if (order && order->m_remaining > Quantity(0)) {
            order->cancel();
            auto ordersOnSide = order->m_side == Order::Side::BUY ? &m_bids : &m_asks;
            if (ordersOnSide && order->isOnList()) {
                ordersOnSide->removeOrder(*order);
                m_listener.onOrder(*order);
                return 0;
            } else {
                return -1;
            }
        } else {
            return -1;
        }
    }

    QuoteOrders getQuotes(SessionIdView sessionId, QuoteIdView quoteId, std::function<QuoteOrders()> createOrders) {
        auto key = SessionQuoteId(std::string(sessionId), quoteId);
        auto it = m_quotes.find(key);
        if (it == m_quotes.end()) {
            return m_quotes.emplace(key, createOrders()).first->second;
        } else {
            return it->second;
        }
    }

    void quote(const QuoteOrders& quotes, Price bidPrice, Quantity bidQuantity, Price askPrice, Quantity askQuantity) {
        auto bid = quotes.m_bid;
        auto ask = quotes.m_ask;
        if (bid->isOnList()) m_bids.removeOrder(*bid);
        if (ask->isOnList()) m_asks.removeOrder(*ask);
        if (bidQuantity != Quantity(0)) {
            bid->m_price = bidPrice;
            bid->m_quantity = bidQuantity;
            bid->m_remaining = bidQuantity;
            bid->m_filled = Quantity(0);
            m_bids.insertOrder(*bid);
            matchOrders(Order::Side::BUY);
        }
        if (askQuantity != Quantity(0)) {
            ask->m_price = askPrice;
            ask->m_quantity = askQuantity;
            ask->m_remaining = askQuantity;
            ask->m_filled = Quantity(0);
            m_asks.insertOrder(*ask);
            matchOrders(Order::Side::SELL);
        }
    }

    const Book getBook() const {
        Book orderBookSnapshot;
        auto snap = [](const PriceLevels& src, std::vector<BookLevel>& dst, std::vector<ExchangeId>& oids) {
            auto fn = [&](const OrderList* orders) {
                Quantity quantity(0);
                for (const Order& order : *orders) {
                    quantity = quantity + order.remainingQuantity();
                    oids.push_back(order.m_exchangeId);
                }
                dst.push_back({orders->m_price, quantity});
            };
            src.forEach(fn);
        };
        orderBookSnapshot.m_bids.reserve(m_bids.size());
        orderBookSnapshot.m_asks.reserve(m_asks.size());
        snap(m_bids, orderBookSnapshot.m_bids, orderBookSnapshot.m_bidOrderIds);
        snap(m_asks, orderBookSnapshot.m_asks, orderBookSnapshot.m_askOrderIds);
        return orderBookSnapshot;
    }

    const Order getOrder(std::shared_ptr<Order> order) {
        if (!order) throw std::invalid_argument(std::string(EngineConstants::kOrderCannotBeNull));
        return *order;
    }

    std::vector<std::string> instruments() const { return {m_instrument}; }
    auto getBidView() const {
        return m_bids.levels() | std::views::transform([](const auto& entry) {
            return BookLevel{entry.price(), entry.totalQuantity()};
        });
    }
    auto getAskView() const {
        return m_asks.levels() | std::views::transform([](const auto& entry) {
            return BookLevel{entry.price(), entry.totalQuantity()};
        });
    }
};

// ============================================================================
// BookMap
// ============================================================================

template <typename TListener>
class BookMap {
    std::atomic<std::shared_ptr<OrderBook<TListener>>> m_orderBooks[kMaxInstruments];
public:
    BookMap() {
        for (ObjectCount i = 0; i < kMaxInstruments; i++) {
            m_orderBooks[i].store(nullptr);
        }
    }
    std::shared_ptr<OrderBook<TListener>> getOrCreate(InstrumentSymbolView instrument, TListener& listener) {
        auto hash = std::hash<std::string_view>{}(instrument);
        const auto start = hash % kMaxInstruments;
        auto orderBook = m_orderBooks[start].load();
        if (orderBook != nullptr && orderBook->m_instrument == instrument) return orderBook;
        auto new_book = std::make_shared<OrderBook<TListener>>(std::string(instrument), listener);
        auto index = start;
        while (true) {
            if (orderBook != nullptr) {
                index = (index + 1) % kMaxInstruments;
                if (index == start) throw std::runtime_error("no room in books map");
                orderBook = m_orderBooks[index].load();
                if (orderBook != nullptr && orderBook->m_instrument == instrument) return orderBook;
            } else {
                if (m_orderBooks[index].compare_exchange_weak(orderBook, new_book)) return new_book;
            }
        }
    }
    std::shared_ptr<OrderBook<TListener>> getOrderBook(InstrumentSymbolView instrument) const {
        auto hash = std::hash<std::string_view>{}(instrument);
        const auto start = hash % kMaxInstruments;
        auto orderBook = m_orderBooks[start].load();
        if (orderBook != nullptr && orderBook->m_instrument == instrument) return orderBook;
        auto index = start;
        while (true) {
            if (orderBook != nullptr) {
                index = (index + 1) % kMaxInstruments;
                if (index == start) return nullptr;
                orderBook = m_orderBooks[index].load();
                if (orderBook != nullptr && orderBook->m_instrument == instrument) return orderBook;
            } else {
                index = (index + 1) % kMaxInstruments;
                if (index == start) return nullptr;
                orderBook = m_orderBooks[index].load();
            }
        }
    }
    std::vector<std::string> instruments() const {
        std::vector<std::string> result;
        for (ObjectCount i = 0; i < kMaxInstruments; i++) {
            auto orderBook = m_orderBooks[i].load();
            if (orderBook != nullptr) result.push_back(orderBook->m_instrument);
        }
        return result;
    }
};

using OrderResult = OrderInsertResult;
using CancelResult = bool;

// ============================================================================
// Exchange
// ============================================================================

template <typename TListener>
class Exchange {
public:
    struct NoOpListener : ExchangeListener {};
    Exchange() : m_listener(s_defaultNoOpListener) {}
    explicit Exchange(TListener& listener) : m_listener(listener) {}
    OrderResult placeBuyOrder(SessionIdView sessionId, InstrumentSymbolView instrument, Price price, Quantity quantity, OrderIdStrView orderId = "") {
        return insertOrderInternal(sessionId, instrument, price, quantity, Order::Side::BUY, orderId);
    }
    OrderResult placeMarketBuyOrder(SessionIdView sessionId, InstrumentSymbolView instrument, Quantity quantity, OrderIdStrView orderId = "") {
        return placeBuyOrder(sessionId, instrument, Price(kMarketBuyPrice), quantity, orderId);
    }
    OrderResult placeSellOrder(SessionIdView sessionId, InstrumentSymbolView instrument, Price price, Quantity quantity, OrderIdStrView orderId = "") {
        return insertOrderInternal(sessionId, instrument, price, quantity, Order::Side::SELL, orderId);
    }
    OrderResult placeMarketSellOrder(SessionIdView sessionId, InstrumentSymbolView instrument, Quantity quantity, OrderIdStrView orderId = "") {
        return placeSellOrder(sessionId, instrument, Price(kMarketSellPrice), quantity, orderId);
    }
    void quote(SessionIdView sessionId, InstrumentSymbolView instrument, Price bidPrice, Quantity bidQuantity, Price askPrice, Quantity askQuantity, QuoteIdView quoteId) {
        auto book = m_books.getOrCreate(instrument, m_listener);
        auto orders = book->getQuotes(sessionId, quoteId, [&]() { return QuoteOrders{}; });
        book->quote(orders, bidPrice, bidQuantity, askPrice, askQuantity);
    }
    CancelResult cancelOrder(ExchangeId exchangeId, SessionIdView sessionId) {
        auto order = m_allOrders.get(exchangeId);
        if (!order) return false;
        auto book = m_books.getOrderBook(InstrumentSymbolView(order->instrument()));
        if (!book) return false;
        book->cancelOrder(order);
        m_allOrders.remove(exchangeId);
        return true;
    }
    std::optional<Book> getBook(InstrumentSymbolView instrument) const {
        auto book = m_books.getOrderBook(instrument);
        if (!book) return std::nullopt;
        return book->getBook();
    }
    std::optional<Order> getOrder(ExchangeId exchangeId) const {
        auto order = m_allOrders.get(exchangeId);
        if (!order) return std::nullopt;
        return *order;
    }
    auto getAllOrders() const { return m_allOrders.all() | std::views::as_const; }
    auto getInstruments() const { return m_books.instruments() | std::views::as_const; }
    void onOrder(const Order& order) { m_listener.onOrder(order); }
    void onTrade(const Trade& trade) { m_listener.onTrade(trade); }
    Guard lock() { return Guard(m_mu); }
    std::vector<InstrumentSymbol> instruments() { return m_books.instruments(); }
    std::vector<std::shared_ptr<const Order>> orders() { return m_allOrders.all(); }
private:
    BookMap<Exchange<TListener>> m_books;
    OrderMap m_allOrders;
    SpinLock m_mu;
    ExchangeId nextId() {
        static std::atomic<ExchangeId> s_nextId{1};
        return s_nextId.fetch_add(1, std::memory_order_relaxed);
    }
    OrderResult insertOrderInternal(SessionIdView sessionId, InstrumentSymbolView instrument, Price price, Quantity quantity, Order::Side side, OrderIdStrView orderId) {
        auto id = nextId();
        auto order = Order::create(sessionId, orderId, instrument, price, quantity, side, id);
        if (!order) {
            return std::unexpected(ErrorContext(InsertError::NullOrder, EngineConstants::kOrderCannotBeNull));
        }
        m_allOrders.add(order);
        auto book = m_books.getOrCreate(instrument, m_listener);
        return book->insertOrder(order);
    }
    TListener& m_listener;
private:
    static inline NoOpListener s_defaultNoOpListener;
};

} // namespace orderbook

// using declarations for convenience (global scope, not exported from module)
// These are for non-module consumers; module consumers use orderbook:: prefix
