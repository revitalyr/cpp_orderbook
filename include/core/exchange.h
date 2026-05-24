/**
 * @file exchange.h
 * @brief High-level Exchange interface coordinating multiple OrderBooks and global order tracking.
 */
#pragma once

#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include "order.h"
#include "orderbook.h"
#include "bookmap.h"
#include "spinlock.h"
#include "ordermap.h"

namespace orderbook {

using CancelResult = bool;

/**
 * @class Exchange
 * @brief The primary entry point for the trading engine.
 *
 * Manages a collection of OrderBooks keyed by instrument and maintains a global
 * map of all active orders for efficient O(1) lookups and cancellations.
 */
template <typename TListener>
class Exchange {
public:
    struct NoOpListener : ExchangeListener {};

    Exchange() : m_listener(s_defaultNoOpListener) {}
    explicit Exchange(TListener& listener) : m_listener(listener) {}

    // -----------------------------------------------------------------------
    // Order placement
    // -----------------------------------------------------------------------

    /**
     * @brief Place a limit buy order.
     * @return ExchangeId on success, unexpected(ErrorContext) on failure.
     */
    OrderInsertResult placeBuyOrder(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Price price,
        Quantity quantity,
        OrderIdStrView orderId = ""
    ) {
        return insertOrderInternal(sessionId, instrument, price, quantity, Order::Side::BUY, orderId);
    }

    /** @brief Place a market buy order (uses internal market-buy sentinel price). */
    OrderInsertResult placeMarketBuyOrder(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Quantity quantity,
        OrderIdStrView orderId = ""
    ) {
        return placeBuyOrder(sessionId, instrument, Price(orderbook::kMarketBuyPrice), quantity, orderId);
    }

    /** @brief Place a limit sell order. */
    OrderInsertResult placeSellOrder(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Price price,
        Quantity quantity,
        OrderIdStrView orderId = ""
    ) {
        return insertOrderInternal(sessionId, instrument, price, quantity, Order::Side::SELL, orderId);
    }

    /** @brief Place a market sell order (uses internal market-sell sentinel price). */
    OrderInsertResult placeMarketSellOrder(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Quantity quantity,
        OrderIdStrView orderId = ""
    ) {
        return placeSellOrder(sessionId, instrument, orderbook::Price(orderbook::kMarketSellPrice), quantity, orderId);
    }

    // -----------------------------------------------------------------------
    // Quoting
    // -----------------------------------------------------------------------

    /**
     * @brief Update or create a two-sided quote.
     *
     * Removes any previous orders at the same quote key, then inserts new
     * bid/ask orders at the given prices. Immediately matches against the
     * opposite side after each insertion.
     */
    void quote(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Price bidPrice, Quantity bidQuantity,
        Price askPrice, Quantity askQuantity,
        QuoteIdView quoteId
    ) {
        auto book = m_books.getOrCreate(instrument, *this);
        auto orders = book->getQuotes(sessionId, quoteId, [&]() { return QuoteOrders{}; });
        book->quote(orders, bidPrice, bidQuantity, askPrice, askQuantity);
    }

    // -----------------------------------------------------------------------
    // Cancellation
    // -----------------------------------------------------------------------

    /**
     * @brief Cancel an existing order by its ExchangeId.
     * @param exchangeId The ID returned by the initial placement.
     * @param sessionId  Session identifier for ownership validation.
     * @return true if the order was found and cancelled.
     */
    CancelResult cancelOrder(ExchangeId exchangeId, SessionIdView sessionId) {
        auto order = m_allOrders.get(exchangeId);
        if (!order) return false;
        auto book = m_books.getOrderBook(InstrumentSymbolView(order->instrument()));
        if (!book) return false;
        book->cancelOrder(order);
        m_allOrders.remove(exchangeId);
        return true;
    }

    // -----------------------------------------------------------------------
    // Read-only queries
    // -----------------------------------------------------------------------

    /** @brief Snapshot of the order book for @p instrument. */
    std::optional<Book> getBook(InstrumentSymbolView instrument) const {
        auto book = m_books.getOrderBook(instrument);
        if (!book) return std::nullopt;
        return book->getBook();
    }

    /** @brief Copy of an order's current state. */
    std::optional<Order> getOrder(ExchangeId exchangeId) const {
        auto order = m_allOrders.get(exchangeId);
        if (!order) return std::nullopt;
        return *order;
    }

    /** @brief Range-based view of all tracked orders. */
    auto getAllOrders() const {
        return m_allOrders.all() | std::views::as_const;
    }

    /** @brief Range-based view of all active instrument symbols. */
    auto getInstruments() const {
        return m_books.instruments() | std::views::as_const;
    }

    // -----------------------------------------------------------------------
    // Listener callbacks (forwarded from OrderBook)
    // -----------------------------------------------------------------------

    void onOrder(const Order& order) { m_listener.onOrder(order); }
    void onTrade(const Trade& trade) { m_listener.onTrade(trade); }

    // -----------------------------------------------------------------------
    // Synchronisation
    // -----------------------------------------------------------------------

    /** @brief Lock guard for external transactional operations. */
    Guard lock() { return Guard(m_mu); }

    std::vector<InstrumentSymbol> instruments() { return m_books.instruments(); }
    std::vector<std::shared_ptr<const Order>> orders() { return m_allOrders.all(); }

private:
    BookMap<Exchange<TListener>> m_books;
    OrderMap m_allOrders;
    SpinLock m_mu;

    static inline NoOpListener s_defaultNoOpListener;

    TListener& m_listener;

    ExchangeId nextId() {
        static std::atomic<ExchangeId> s_nextId{1};
        return s_nextId.fetch_add(1, std::memory_order_relaxed);
    }

    OrderInsertResult insertOrderInternal(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Price price,
        Quantity quantity,
        Order::Side side,
        OrderIdStrView orderId
    ) {
        auto id = nextId();
        auto order = Order::create(sessionId, orderId, instrument, price, quantity, side, id);
        if (!order)
            return std::unexpected(ErrorContext(InsertError::NullOrder, "Order cannot be null"));

        m_allOrders.add(order);
        auto book = m_books.getOrCreate(instrument, *this);
        return book->insertOrder(order);
    }
};

} // namespace orderbook
