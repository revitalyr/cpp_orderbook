#pragma once

#include <chrono>        // For std::chrono::system_clock::now()
#include <format>        // For std::format
#include <iostream>      // For std::ostream
#include <list>          // For std::list
#include <map>           // For std::map
#include <memory>        // For std::shared_ptr, std::weak_ptr
#include <ranges>        // For std::views::transform
#include <source_location> // For std::source_location
#include <stdexcept>     // For std::invalid_argument, std::runtime_error
#include <string>        // For std::string
#include <vector>        // For std::vector

#include "spinlock.h"
#include "pricelevels.h"
#include "insert_result.h"
#include "semantic_types.h"
#include "constants.h"

 #include "safety/production_safety.h" // Moved after module imports
namespace orderbook {

/**
 * @brief Interface for receiving asynchronous trade and order events.
 */
struct ExchangeListener {
    /** @brief Triggered whenever an order state changes (insertion, cancellation, etc.) */
    virtual void onOrder(const Order& ) {}
    /** @brief Triggered when a match occurs between a bid and an ask */
    virtual void onTrade(const struct Trade& ) {}
};

// C++20 Concept to validate the listener interface at compile time
template <typename T>
concept Listener = requires(T listener, const struct Order& order, const struct Trade& trade) {
    { listener.onOrder(order) } -> std::same_as<void>;
    { listener.onTrade(trade) } -> std::same_as<void>; // Renamed to camelCase
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
    /** execution timestamp in nanoseconds since epoch */
    const NanosecondTimestamp execId;
};

struct BookLevel {
    Price m_price; // Renamed to m_snake_case
    Quantity m_quantity; // Renamed to m_snake_case
};

struct Book {
    std::vector<BookLevel> m_bids; // Renamed to m_snake_case
    std::vector<ExchangeId> m_bidOrderIds; // Renamed to m_snake_case
    std::vector<BookLevel> m_asks; // Renamed to m_snake_case
    std::vector<ExchangeId> m_askOrderIds; // Renamed to m_snake_case
};

inline std::ostream& operator<<(std::ostream& os, const Book& book) {
    bool first = true;
    for (auto side : {book.m_asks, book.m_bids}) {
        for(auto level : side) {
            os << level.m_price << " " << level.m_quantity << "\n";
        }
        if(first) { os << "----------\n"; first=false; }
    }
    return os;
}

// map of Session+QuoteId to the associated orders, or null if no quote on that side
struct QuoteOrders { // Renamed to PascalCase
    std::shared_ptr<Order> m_bid = nullptr; // Renamed to m_snake_case
    std::shared_ptr<Order> m_ask = nullptr; // Renamed to m_snake_case
};

struct SessionQuoteId {
    const std::string m_sessionId; // Renamed to m_snake_case
    const std::string m_quoteId; // Renamed to m_snake_case
    SessionQuoteId(const std::string& sessionId, const std::string_view& quoteId) : m_sessionId(sessionId), m_quoteId(quoteId){} // Renamed to m_snake_case
    bool operator<(const SessionQuoteId& other) const {
        return m_sessionId < other.m_sessionId || (m_sessionId == other.m_sessionId && m_quoteId < other.m_quoteId); // Renamed to m_snake_case
    }
    bool operator==(const SessionQuoteId& other) const {
        return m_sessionId == other.m_sessionId && m_quoteId == other.m_quoteId; // Renamed to m_snake_case
    }
};

inline std::ostream& operator<<(std::ostream& os, const SessionQuoteId& id) {
    return os << "[" << id.m_sessionId << ":" << id.m_quoteId << "]";
}

// Forward declaration must be a template to match the actual definition
template <typename TListener> class Exchange;

/** OrderBook instances are single threaded and must be externally synchronized using mu or lock() */
template <typename TListener>
class OrderBook {
private: // Internal state and helper methods
    SpinLock m_mu; // Mutex for external synchronization // Renamed to m_snake_case
    PriceLevels m_bids = PriceLevels(false); // Bid price levels (descending) // Renamed to m_snake_case
    PriceLevels m_asks = PriceLevels(true); // Ask price levels (ascending) // Renamed to m_snake_case
    TListener& m_listener; // Listener for trade and order events // Renamed to m_snake_case
    void matchOrders(Order::Side aggressorSide); // Attempts to match orders
    std::map<SessionQuoteId,QuoteOrders> m_quotes; // Map of session/quote ID to active quotes // Renamed to m_snake_case
    
public: // Public interface
    const std::string m_instrument; // The instrument this order book is for // Renamed to m_snake_case
    OrderBook(const std::string &instrument, TListener& listener) : m_listener(listener), m_instrument(instrument) {} // Renamed to m_snake_case
    ~OrderBook() = default;

    // C++20: Modern insertOrder with explicit error handling and stack overflow protection // Renamed to camelCase
    OrderInsertResult insertOrder(
        std::shared_ptr<Order> order,
        std::source_location location = std::source_location::current()
    );
    
    // Legacy overload for backward compatibility (deprecated)
    void insertOrderLegacy(std::shared_ptr<Order> order);
    
    int cancelOrder(std::shared_ptr<Order> order);

    QuoteOrders getQuotes(SessionIdView sessionId, QuoteIdView quoteId, std::function<QuoteOrders()> createOrders);
    void quote(const QuoteOrders& quotes, Price bidPrice, Quantity bidQuantity, Price askPrice, Quantity askQuantity);

    const Book getBook() const;
    const Order getOrder(std::shared_ptr<Order> order);
    std::vector<std::string> instruments() const {
        return {m_instrument}; // Renamed to m_snake_case
    }

    /** @brief Returns a range-based view of the bid side. Lock must be held during iteration. */
    auto getBidView() const {
        return m_bids.levels() | std::views::transform([](const OrderList& list) {
            return BookLevel{list.price(), list.totalQuantity()}; // Теперь O(1)
        });
    }

    /** @brief Returns a range-based view of the ask side. Lock must be held during iteration. */
    auto getAskView() const {
        return m_asks.levels() | std::views::transform([](const OrderList& list) {
            return BookLevel{list.price(), list.totalQuantity()}; // Теперь O(1)
        });
    }
};

// Implementation of templated OrderBook methods
template <typename TListener>
OrderInsertResult OrderBook<TListener>::insertOrder(
    std::shared_ptr<Order> order,
    std::source_location location
) {
    // Prevents stack exhaustion during highly recursive matching/callback scenarios
    ::ProductionSafety::CriticalGuard stack_guard;
    
    if (!stack_guard.isValid()) {
        orderbook::ErrorContext error(orderbook::InsertError::StackOverflowProtection, orderbook::EngineConstants::kRecursionDepthExceeded, location); // Renamed to kPascalCase
        error.recursion_depth = orderbook::StackProtection::currentDepth();
        return std::unexpected(error);
    }
    
    if (!order) {
        return std::unexpected(orderbook::ErrorContext(orderbook::InsertError::NullOrder, orderbook::EngineConstants::kOrderPointerNull, location));
    }
    
    if (order->remainingQuantity() <= 0) {
        return std::unexpected(orderbook::ErrorContext(orderbook::InsertError::InvalidQuantity, 
            std::format("{}{}", orderbook::EngineConstants::kInvalidOrderQuantity, order->remainingQuantity()), location));
    }
    
    if (order->isOnList()) {
        return std::unexpected(orderbook::ErrorContext(orderbook::InsertError::OrderAlreadyExists,
            std::format("{} ID: {}", orderbook::EngineConstants::kOrderAlreadyOnList, order->m_exchangeId), location));
    }
    
    auto orderList = order->m_side == Order::Side::BUY ? &m_bids : &m_asks;
    
    // Insert the order
    orderList->insertOrder(order);
    m_listener.onOrder(*order);
    
    // Perform immediate execution against opposite side
    matchOrders(order->m_side);
    
    return order->m_exchangeId;
}

template <typename TListener>
void OrderBook<TListener>::insertOrderLegacy(std::shared_ptr<Order> order) {
    insertOrder(order);
}

template <typename TListener>
void OrderBook<TListener>::matchOrders(Order::Side aggressorSide) {
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    while (!m_bids.empty() && !m_asks.empty()) {
        auto bid = m_bids.front();
        auto ask = m_asks.front();

        if (bid->isMarket() || ask->isMarket() || bid->m_price >= ask->m_price) {
            Quantity qty = std::min(bid->m_remaining, ask->m_remaining);
            // Trade at the price of the resting order
            Price price = (aggressorSide == Order::Side::BUY) ? ask->m_price : bid->m_price;

            std::shared_ptr<Order> aggressor = aggressorSide == Order::Side::BUY ? bid : ask;
            std::shared_ptr<Order> opposite = aggressorSide == Order::Side::BUY ? ask : bid;

            bid->fill(qty,price); // Renamed to camelCase
            ask->fill(qty,price);

            const Trade trade(price, qty, *aggressor, *opposite, now_ns);

            if (bid->m_remaining == 0) {
                m_bids.removeOrder(bid);
            }
            if (ask->m_remaining == 0) {
                m_asks.removeOrder(ask);
            }
            m_listener.onOrder(*bid);
            m_listener.onOrder(*ask);
            m_listener.onTrade(trade);
        } else {
            break;
        }
    }
    // Cancel any unexecuted volume for market orders
    auto ordersOnSide = aggressorSide == Order::Side::BUY ? &m_bids : &m_asks;
    if (!ordersOnSide->empty()) {
        auto order = ordersOnSide->front();
        if (order && order->isMarket()) { // Renamed to camelCase
            order->cancel();
            ordersOnSide->removeOrder(order);
            m_listener.onOrder(*order);
        }
    }
}

template <typename TListener>
QuoteOrders OrderBook<TListener>::getQuotes(SessionIdView sessionId, QuoteIdView quoteId, std::function<QuoteOrders()> createOrders) {
    auto key = SessionQuoteId(std::string(sessionId), quoteId);
    auto it = m_quotes.find(key);
    if (it == m_quotes.end()) {
        return m_quotes.emplace(key, createOrders()).first->second;
    } else {
        return it->second;
    }
}

template <typename TListener>
void OrderBook<TListener>::quote(const QuoteOrders& quotes, Price bidPrice, Quantity bidQuantity, Price askPrice, Quantity askQuantity) {
    auto bid = quotes.m_bid;
    auto ask = quotes.m_ask;
    if(bid->isOnList()) {
        m_bids.removeOrder(bid);
    }
    if(ask->isOnList()) {
        m_asks.removeOrder(ask);
    }
    if (bidQuantity != Quantity(0)) {
        bid->m_price = bidPrice;
        bid->m_quantity = bidQuantity;
        bid->m_remaining = bidQuantity;
        bid->m_filled = Quantity(0);
        m_bids.insertOrder(bid);
        matchOrders(Order::Side::BUY);
    }
    if (askQuantity != Quantity(0)) {
        ask->m_price = askPrice;
        ask->m_quantity = askQuantity;
        ask->m_remaining = askQuantity;
        ask->m_filled = Quantity(0);
        m_asks.insertOrder(ask);
        matchOrders(Order::Side::SELL);
    }
}

template <typename TListener>
int OrderBook<TListener>::cancelOrder(std::shared_ptr<Order> order) {
    if (!order) {
        return -1;
    }
    
    if (order && order->m_remaining > Quantity(0)) {
        order->cancel();
        auto ordersOnSide = order->m_side == Order::Side::BUY ? &m_bids : &m_asks;
        
        // Add safety check before removal
        if (ordersOnSide && order->isOnList()) {
            ordersOnSide->removeOrder(order);
            m_listener.onOrder(*order);
            return 0;
        } else {
            // Order not found in lists or not on list
            return -1;
        }
    } else {
        return -1;
    }
}

template <typename TListener>
const Book OrderBook<TListener>::getBook() const {
    Book orderBookSnapshot;
    auto snap = [](const PriceLevels& src, std::vector<BookLevel>& dst, std::vector<ExchangeId>& oids) {
        auto fn = [&](const OrderList* orders) {
            Quantity quantity(0);
            for (auto itr = orders->begin(); itr != orders->end(); ++itr) {
                auto order = *itr;
                if (order) {
                    quantity = quantity + order->remainingQuantity();
                    oids.push_back(order->m_exchangeId);
                }
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
    if (!order) {
        throw std::invalid_argument(std::string(orderbook::EngineConstants::kOrderCannotBeNull));
    }
    return *order;
}

} // namespace orderbook