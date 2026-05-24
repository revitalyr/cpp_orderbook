#pragma once

#include <atomic>
#include <cfloat>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include <boost/intrusive/list_hook.hpp>
#include <boost/intrusive/link_mode.hpp>

#include "string_interner.h"
#include "memory_pool.h"
#include "semantic_types.h"

namespace orderbook {

inline const Price kMarketBuyPrice = Price(1000000000);
inline const Price kMarketSellPrice = Price(-1000000000);

inline Timestamp epoch() {
    return std::chrono::system_clock::now();
}

} // namespace orderbook

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

/**
 * @struct Order
 * @brief Highly optimised, cache-line-aligned order representation.
 *
 * Members are ordered to minimise padding. Session and instrument IDs are
 * stored as 32-bit interned integers. Hot data (price, quantity, links) is
 * grouped at the front of the struct for better cache utilisation.
 */
namespace orderbook {

using StringId = StringInterner::StringId;

struct alignas(64) Order {
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

    /**
     * @brief Factory method — allocates from the global OrderPool.
     */
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
    // Intrusive list hook for OrderList membership
    boost::intrusive::list_member_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>> m_listHook;
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
        m_filled += quantity;
        m_averagePrice = (m_averagePrice * m_cumulativeQuantity + price * quantity)
                       / (m_cumulativeQuantity + quantity);
        m_cumulativeQuantity += quantity;
    }

    void cancel() noexcept { m_remaining = Quantity(0); }

    [[nodiscard]] bool isMarket() const noexcept {
        return m_price == orderbook::kMarketBuyPrice || m_price == orderbook::kMarketSellPrice;
    }

public:
    [[nodiscard]] std::string sessionId() const {
        return std::string(g_globalStringInterner().get(m_sessionId));
    }

    [[nodiscard]] std::string orderId() const {
        if (m_orderId != StringInterner::INVALID_ID)
            return std::string(g_globalStringInterner().get(m_orderId));
        return std::to_string(m_orderIdNum);
    }

    [[nodiscard]] std::string instrument() const {
        return std::string(g_globalStringInterner().get(m_instrumentId));
    }

    [[nodiscard]] ExchangeId orderIdNum() const noexcept { return m_orderIdNum; }
    [[nodiscard]] Price price() const noexcept { return m_price; }
    [[nodiscard]] Quantity quantity() const noexcept { return m_quantity; }
    [[nodiscard]] bool isOnList() const noexcept { return m_onList; }

    [[nodiscard]] Quantity remainingQuantity() const noexcept { return m_remaining; }
    [[nodiscard]] Quantity filledQuantity() const noexcept { return m_filled; }
    [[nodiscard]] Quantity cumulativeQuantity() const noexcept { return m_cumulativeQuantity; }
    [[nodiscard]] Price averagePrice() const noexcept { return m_averagePrice; }

    [[nodiscard]] bool isCancelled() const noexcept {
        return m_remaining == Quantity(0) && m_filled != m_quantity;
    }
    [[nodiscard]] bool isFilled() const noexcept {
        return m_remaining == Quantity(0) && m_filled == m_quantity;
    }
    [[nodiscard]] bool isPartiallyFilled() const noexcept {
        return m_remaining == Quantity(0) && m_filled > Quantity(0);
    }
    [[nodiscard]] bool isActive() const noexcept { return m_remaining > 0; }

    /// Copy constructor — `m_nextPtr` is atomic and not copied (reset to nullptr).
    Order(const Order& other)
        : m_onList(false),
          m_nextPtr(nullptr),
          m_timeSubmitted(other.m_timeSubmitted),
          m_orderIdNum(other.m_orderIdNum),
          m_price(other.m_price),
          m_averagePrice(other.m_averagePrice),
          m_remaining(other.m_remaining),
          m_filled(other.m_filled),
          m_quantity(other.m_quantity),
          m_cumulativeQuantity(other.m_cumulativeQuantity),
          m_sessionId(other.m_sessionId),
          m_orderId(other.m_orderId),
          m_instrumentId(other.m_instrumentId),
          m_exchangeId(other.m_exchangeId),
          m_side(other.m_side),
          m_isQuote(other.m_isQuote) {}

public:
    /**
     * @brief Construct an Order with interned strings.
     *
     * Numeric orderId strings are parsed directly into m_orderIdNum to avoid
     * interning overhead for auto-generated IDs.
     */
    Order(SessionIdView sessionId, OrderIdStrView orderId,
           InstrumentSymbolView instrument, Price price, Quantity quantity,
           Order::Side side, ExchangeId exchangeId)
         : m_listHook(),
           m_onList(false),
           m_timeSubmitted(epoch()),
           m_price(price),
           m_remaining(quantity),
           m_quantity(quantity),
           m_sessionId(orderbook::g_globalStringInterner().intern(sessionId)),
           m_instrumentId(orderbook::g_globalStringInterner().intern(instrument)),
           m_exchangeId(exchangeId),
           m_side(side)
    {
        if (!orderId.empty() && std::isdigit(static_cast<unsigned char>(orderId[0]))) {
            auto [ptr, ec] = std::from_chars(orderId.data(),
                orderId.data() + orderId.size(), m_orderIdNum);
            if (ec != std::errc()) {
                m_orderIdNum = 0;
                m_orderId = orderbook::g_globalStringInterner().intern(orderId);
            }
        } else if (!orderId.empty()) {
            m_orderId = orderbook::g_globalStringInterner().intern(orderId);
            m_orderIdNum = 0;
        }
    }
};

} // namespace orderbook

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace orderbook {

/**
 * @brief Thread-safe singleton wrapping a `MemoryPool<Order>`.
 *
 * Usage:
 * @code
 *   OrderPool::reserve(1'000'000);   // pre-allocate
 *   auto order = Order::create(...); // allocated from pool
 * @endcode
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

inline std::shared_ptr<orderbook::Order> orderbook::Order::create(
    orderbook::SessionIdView sessionId,
    orderbook::OrderIdStrView orderId,
    orderbook::InstrumentSymbolView instrument,
    orderbook::Price price,
    orderbook::Quantity quantity,
    orderbook::Order::Side side,
    orderbook::ExchangeId exchangeId
) {
    using Allocator = orderbook::MemoryPoolAllocator<orderbook::Order, orderbook::OrderPool>;
    return std::allocate_shared<orderbook::Order>(Allocator{},
        sessionId, orderId, instrument, price, quantity, side, exchangeId);
}
