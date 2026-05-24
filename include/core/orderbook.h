#pragma once

#include <chrono>
#include <format>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <ranges>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

#include "spinlock.h"
#include "pricelevels.h"
#include "insert_result.h"
#include "semantic_types.h"

#include "safety/production_safety.h"

namespace orderbook {

/**
 * @brief Interface for receiving asynchronous trade and order events.
 */
struct ExchangeListener {
    virtual void onOrder(const Order& ) {}
    virtual void onTrade(const struct Trade& ) {}
};

/// Validates that a listener type satisfies the ExchangeListener interface.
template <typename T>
concept Listener = requires(T listener, const struct Order& order, const struct Trade& trade) {
    { listener.onOrder(order) } -> std::same_as<void>;
    { listener.onTrade(trade) } -> std::same_as<void>;
};

/**
 * @brief A single trade (match) between an aggressor and a resting order.
 */
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
    const NanosecondTimestamp execId; /// Execution timestamp (ns since epoch).
};

/**
 * @brief Aggregated price level for book snapshots.
 */
struct BookLevel {
    Price m_price;
    Quantity m_quantity;
};

/**
 * @brief Full order-book snapshot.
 */
struct Book {
    std::vector<BookLevel> m_bids;
    std::vector<ExchangeId> m_bidOrderIds;
    std::vector<BookLevel> m_asks;
    std::vector<ExchangeId> m_askOrderIds;
};

inline std::ostream& operator<<(std::ostream& os, const Book& book) {
    bool first = true;
    for (auto side : {book.m_asks, book.m_bids}) {
        for (auto level : side)
            os << level.m_price << " " << level.m_quantity << "\n";
        if (first) { os << "----------\n"; first = false; }
    }
    return os;
}

/**
 * @brief Bid/ask pair belonging to a single quote.
 */
struct QuoteOrders {
    std::shared_ptr<Order> m_bid = nullptr;
    std::shared_ptr<Order> m_ask = nullptr;
};

/**
 * @brief Composite key for per-session quote identification.
 */
struct SessionQuoteId {
    const std::string m_sessionId;
    const std::string m_quoteId;

    SessionQuoteId(const std::string& sessionId, const std::string_view& quoteId)
        : m_sessionId(sessionId), m_quoteId(quoteId) {}

    bool operator<(const SessionQuoteId& other) const {
        return m_sessionId < other.m_sessionId
            || (m_sessionId == other.m_sessionId && m_quoteId < other.m_quoteId);
    }
    bool operator==(const SessionQuoteId& other) const {
        return m_sessionId == other.m_sessionId && m_quoteId == other.m_quoteId;
    }
};

inline std::ostream& operator<<(std::ostream& os, const SessionQuoteId& id) {
    return os << "[" << id.m_sessionId << ":" << id.m_quoteId << "]";
}

template <typename TListener> class Exchange;

/**
 * @brief Single-instrument limit-order book.
 *
 * **Thread safety**: `OrderBook` is **not** re-entrant. External synchronisation
 * (e.g. a per-instance mutex in `Exchange`) must be used for concurrent access.
 */
template <typename TListener>
class OrderBook {
private:
    SpinLock m_mu;
    PriceLevels m_bids = PriceLevels(false);   /// Bid levels, descending price.
    PriceLevels m_asks = PriceLevels(true);    /// Ask levels, ascending price.
    TListener& m_listener;
    std::map<SessionQuoteId, QuoteOrders> m_quotes;

    void matchOrders(Order::Side aggressorSide);

public:
    const std::string m_instrument;

    OrderBook(const std::string& instrument, TListener& listener)
        : m_listener(listener), m_instrument(instrument) {}
    ~OrderBook() = default;

    // -----------------------------------------------------------------------
    // Order insertion
    // -----------------------------------------------------------------------

    /**
     * @brief Insert an order and immediately match against the opposite side.
     * @param order A valid, non-null, not-yet-inserted shared_ptr<Order>.
     * @return The ExchangeId on success, or an ErrorContext on failure.
     */
    OrderInsertResult insertOrder(std::shared_ptr<Order> order);

    /**
     * @brief Like insertOrder, but attaches a source_location for error tracing.
     */
    OrderInsertResult insertOrderWithLocation(
        std::shared_ptr<Order> order,
        std::source_location location = std::source_location::current()
    );

    // -----------------------------------------------------------------------
    // Cancellation
    // -----------------------------------------------------------------------

    /// @return 0 on success, -1 on error.
    int cancelOrder(std::shared_ptr<Order> order);

    // -----------------------------------------------------------------------
    // Quoting
    // -----------------------------------------------------------------------

    QuoteOrders getQuotes(SessionIdView sessionId, QuoteIdView quoteId,
                          std::function<QuoteOrders()> createOrders);
    void quote(const QuoteOrders& quotes,
               Price bidPrice, Quantity bidQuantity,
               Price askPrice, Quantity askQuantity);

    // -----------------------------------------------------------------------
    // Book snapshot
    // -----------------------------------------------------------------------

    const Book getBook() const;
    const Order getOrder(std::shared_ptr<Order> order);
    std::vector<std::string> instruments() const { return {m_instrument}; }

    /**
     * @brief Range of bid levels — lock must be held during iteration.
     */
    auto getBidView() const {
        return m_bids.levels()
             | std::views::transform([](const auto& entry) {
                   return BookLevel{entry.price(), entry.totalQuantity()};
               });
    }

    /**
     * @brief Range of ask levels — lock must be held during iteration.
     */
    auto getAskView() const {
        return m_asks.levels()
             | std::views::transform([](const auto& entry) {
                   return BookLevel{entry.price(), entry.totalQuantity()};
               });
    }
};

// ===========================================================================
// Implementation
// ===========================================================================

template <typename TListener>
OrderInsertResult OrderBook<TListener>::insertOrder(std::shared_ptr<Order> order) {
    return insertOrderWithLocation(order);
}

template <typename TListener>
OrderInsertResult OrderBook<TListener>::insertOrderWithLocation(
    std::shared_ptr<Order> order,
    std::source_location location
) {
    ::ProductionSafety::CriticalGuard stack_guard;
    if (!stack_guard.isValid()) {
        auto loc = location;
        orderbook::ErrorContext error(
            orderbook::InsertError::StackOverflowProtection,
            "Recursion depth limit exceeded in insertOrder", loc);
        error.recursion_depth = orderbook::StackProtection::currentDepth();
        return std::unexpected(error);
    }

    if (!order)
        return std::unexpected(
            orderbook::ErrorContext(orderbook::InsertError::NullOrder,
                                    "Order pointer is null",
                                    location));

    if (order->remainingQuantity() <= 0)
        return std::unexpected(
            orderbook::ErrorContext(orderbook::InsertError::InvalidQuantity,
                std::format("Invalid order quantity: {}", order->remainingQuantity()), location));

    if (order->isOnList())
        return std::unexpected(
            orderbook::ErrorContext(orderbook::InsertError::OrderAlreadyExists,
                std::format("Order is already on a list ID: {}", order->m_exchangeId), location));

    auto orderList = order->m_side == Order::Side::BUY ? &m_bids : &m_asks;
    orderList->insertOrder(*order);
    m_listener.onOrder(*order);

    matchOrders(order->m_side);
    return order->m_exchangeId;
}

/**
 * @brief Match the aggressor side against resting orders on the opposite side.
 *
 * Continues until one side is empty or prices no longer cross.
 * Fully-filled orders are removed; market orders are cancelled for any
 * unfilled remainder.
 */
template <typename TListener>
void OrderBook<TListener>::matchOrders(Order::Side aggressorSide) {
    while (!m_bids.empty() && !m_asks.empty()) {
        auto* bid = m_bids.front();
        auto* ask = m_asks.front();

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

template <typename TListener>
QuoteOrders OrderBook<TListener>::getQuotes(
    SessionIdView sessionId, QuoteIdView quoteId,
    std::function<QuoteOrders()> createOrders)
{
    auto key = SessionQuoteId(std::string(sessionId), quoteId);
    auto it = m_quotes.find(key);
    if (it == m_quotes.end())
        return m_quotes.emplace(key, createOrders()).first->second;
    else
        return it->second;
}

template <typename TListener>
void OrderBook<TListener>::quote(
    const QuoteOrders& quotes,
    Price bidPrice, Quantity bidQuantity,
    Price askPrice, Quantity askQuantity)
{
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

template <typename TListener>
int OrderBook<TListener>::cancelOrder(std::shared_ptr<Order> order) {
    if (!order) return -1;

    if (order->m_remaining > Quantity(0)) {
        order->cancel();
        auto ordersOnSide = order->m_side == Order::Side::BUY ? &m_bids : &m_asks;

        if (ordersOnSide && order->isOnList()) {
            ordersOnSide->removeOrder(*order);
            m_listener.onOrder(*order);
            return 0;
        }
        return -1;
    }
    return -1;
}

template <typename TListener>
const Book OrderBook<TListener>::getBook() const {
    Book orderBookSnapshot;

    auto snap = [](const PriceLevels& src,
                   std::vector<BookLevel>& dst,
                   std::vector<ExchangeId>& oids) {
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

template <typename TListener>
const Order OrderBook<TListener>::getOrder(std::shared_ptr<Order> order) {
    if (!order)
        throw std::invalid_argument("Order cannot be null");
    return *order;
}

} // namespace orderbook
