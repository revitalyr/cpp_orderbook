#pragma once

#include <atomic>     // For std::atomic
#include <cfloat>     // For DBL_MAX
#include <charconv>   // For std::from_chars
#include <chrono>     // For std::chrono::system_clock
#include <cstdint>    // For uint8_t
#include <memory>     // For std::shared_ptr, std::weak_ptr
#include <string>     // For std::string

#include "string_interner.h"
#include "memory_pool.h"
#include "semantic_types.h"
#include "constants.h"

namespace orderbook {
/** Helper function for getting current time */
inline Timestamp epoch() {
    return std::chrono::system_clock::now();
}
 }

// ============================================================================
// TYPE ALIASES
// ============================================================================

using StringId = orderbook::StringInterner::StringId;
/**
 * Memory-optimized Order structure
 * - Uses StringInterner for strings (4 bytes each instead of 32+)
 * - Compact Side enum (1 byte)
 * - Aligned for cache efficiency
 */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif
/**
 * @struct Order
 * @brief Highly optimized order representation designed for cache-line alignment.
 * 
 * Members are ordered to minimize padding and group frequently accessed hot data.
 * Uses StringInterning to reduce session and instrument IDs to 32-bit integers.
 */
namespace orderbook {

struct alignas(64) Order { // Moved into orderbook namespace
public:
    enum class Side : uint8_t { BUY = 0, SELL = 1 };

    template <typename TListener> friend class OrderBook;
    friend class OrderList;
    friend class OrderMap;
    template <typename TListener> friend class Exchange;
    friend class TestOrder;
    template<typename> friend class PointerPriceLevels;
    template<typename> friend class StructPriceLevels;
    template<typename> friend class MapPriceLevels;
    template<typename> friend class MapPtrPriceLevels;

    [[nodiscard]] static std::shared_ptr<Order> create(
        SessionIdView sessionId,
        OrderIdStrView orderId,
        InstrumentSymbolView instrument,
        Price price,
        Quantity quantity,
        Order::Side side,
        ExchangeId exchangeId
    );

private:
    // Intrusive list links for OrderList membership
    std::shared_ptr<Order> m_nextList{nullptr};
    std::weak_ptr<Order> m_prevList;
    bool m_onList = false;

    std::atomic<Order*> m_nextPtr{nullptr};
    const orderbook::Timestamp m_timeSubmitted;
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
        m_filled += quantity; // Renamed to camelCase
        m_averagePrice = (m_averagePrice * m_cumulativeQuantity + price * quantity) / (m_cumulativeQuantity + quantity);
        m_cumulativeQuantity += quantity;
    }
    
    void cancel() noexcept { m_remaining = Quantity(0); }
    
    [[nodiscard]] bool isMarket() const noexcept {
        return m_price == orderbook::kMarketBuyPrice || m_price == orderbook::kMarketSellPrice;
    }

public:
    // Public accessor methods
    [[nodiscard]] std::string sessionId() const { 
        return std::string(g_globalStringInterner().get(m_sessionId));
    }

    [[nodiscard]] std::string orderId() const { 
        if (m_orderId != StringInterner::INVALID_ID) {
            return std::string(g_globalStringInterner().get(m_orderId));
        }
        return std::to_string(m_orderIdNum);
    }

    [[nodiscard]] std::string instrument() const { 
        return std::string(g_globalStringInterner().get(m_instrumentId));
    }
    
    [[nodiscard]] ExchangeId orderIdNum() const noexcept { return m_orderIdNum; }
    [[nodiscard]] Price price() const noexcept { return m_price; }
    [[nodiscard]] Quantity quantity() const noexcept { return m_quantity; }

    [[nodiscard]] bool isOnList() const noexcept {
        return m_onList;
    }

    [[nodiscard]] Quantity remainingQuantity() const noexcept { return m_remaining; }
    [[nodiscard]] Quantity filledQuantity() const noexcept { return m_filled; }
    [[nodiscard]] Quantity cumulativeQuantity() const noexcept { return m_cumulativeQuantity; }
    [[nodiscard]] Price averagePrice() const noexcept { return m_averagePrice; }
    
    [[nodiscard]] bool isCancelled() const noexcept { return m_remaining == Quantity(0) && m_filled != m_quantity; }
    [[nodiscard]] bool isFilled() const noexcept { return m_remaining == Quantity(0) && m_filled == m_quantity; }
    [[nodiscard]] bool isPartiallyFilled() const noexcept { return m_remaining == Quantity(0) && m_filled > Quantity(0); }
    [[nodiscard]] bool isActive() const noexcept { return m_remaining > 0; }

    // Copy constructor - atomic next is not copied (initialized to nullptr)
    Order(const Order& other)
        : m_nextList(nullptr),
          m_prevList(),
          m_onList(false),
          m_nextPtr(nullptr), // Renamed to m_snake_case
          m_timeSubmitted(other.m_timeSubmitted), // Renamed to m_snake_case
          m_orderIdNum(other.m_orderIdNum), // Renamed to m_snake_case
          m_price(other.m_price), // Renamed to m_snake_case
          m_averagePrice(other.m_averagePrice), // Renamed to m_snake_case
          m_remaining(other.m_remaining), // Renamed to m_snake_case
          m_filled(other.m_filled), // Renamed to m_snake_case
          m_quantity(other.m_quantity), // Renamed to m_snake_case
          m_cumulativeQuantity(other.m_cumulativeQuantity), // Renamed to m_snake_case
          m_sessionId(other.m_sessionId), // Renamed to m_snake_case
          m_orderId(other.m_orderId), // Renamed to m_snake_case
          m_instrumentId(other.m_instrumentId), // Renamed to m_snake_case
          m_exchangeId(other.m_exchangeId), // Renamed to m_snake_case
          m_side(other.m_side), // Renamed to m_snake_case
          m_isQuote(other.m_isQuote) {} // Renamed to m_snake_case

public:
    // Optimized constructor: avoids exceptions and unnecessary allocations
    Order(SessionIdView sessionId, OrderIdStrView orderId, 
          InstrumentSymbolView instrument, Price price, Quantity quantity, 
          Order::Side side, ExchangeId exchangeId) 
        : m_nextList(nullptr),
          m_prevList(),
          m_onList(false),
          m_timeSubmitted(epoch()), // Renamed to m_snake_case
          m_price(price), // Renamed to m_snake_case
          m_remaining(quantity), // Renamed to m_snake_case
          m_quantity(quantity), // Renamed to m_snake_case
          m_sessionId(orderbook::g_globalStringInterner().intern(sessionId)), // Renamed to m_snake_case, g_camelCase
          m_instrumentId(orderbook::g_globalStringInterner().intern(instrument)), // Renamed to m_snake_case, g_camelCase
          m_exchangeId(exchangeId), // Renamed to m_snake_case
          m_side(side) // Renamed to m_snake_case
    {
        // C++17 fast non-throwing parsing
        if (!orderId.empty() && std::isdigit(static_cast<unsigned char>(orderId[0]))) {
            auto [ptr, ec] = std::from_chars(orderId.data(), orderId.data() + orderId.size(), m_orderIdNum);
            if (ec != std::errc()) { // Renamed to camelCase
                m_orderIdNum = 0;
                m_orderId = orderbook::g_globalStringInterner().intern(orderId);
            }
        } else if (!orderId.empty()) {
            m_orderId = orderbook::g_globalStringInterner().intern(orderId); // Renamed to m_snake_case, g_camelCase
            m_orderIdNum = 0; // Renamed to m_snake_case
        }
    }
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace orderbook {
/**
 * Global order pool - singleton for the application.
 * Defined after Order struct to ensure the type is complete for MemoryPool instantiation.
 */
class OrderPool {
public:
    static MemoryPool<Order>& instance() {
        static MemoryPool<Order> pool;
        return pool;
    }

    static void reserve(size_t n) { instance().reserve(n); }
};

} // namespace orderbook

/**
 * Definition of Order::create must come after OrderPool is defined
 * to resolve dependencies and allow sizeof(Order) in the MemoryPool.
 */
inline std::shared_ptr<Order> orderbook::Order::create(
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
