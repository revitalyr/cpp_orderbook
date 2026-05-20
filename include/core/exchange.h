/**
 * @file exchange.h
 * @brief High-level Exchange interface coordinating multiple OrderBooks and global order tracking.
 */
#pragma once

#include <memory>   // For std::shared_ptr
#include <optional> // For std::optional
#include <ranges>   // For std::views::as_const
#include <string>   // For std::string
#include <vector>   // For std::vector

#include "order.h"
#include "orderbook.h"
#include "bookmap.h"
#include "spinlock.h"
#include "ordermap.h"

namespace orderbook {

using OrderResult = OrderInsertResult; // Alias for consistency with previous code
using CancelResult = bool; // Alias for consistency with previous code

/**
 * @class Exchange
 * @brief The primary entry point for the trading engine.
 * 
 * Manages a collection of OrderBooks keyed by instrument and maintains a global 
 * map of all active orders for efficient O(1) lookups and cancellations.
 */
template <typename TListener> // Renamed to camelCase
class Exchange { // Exchange no longer inherits from OrderBookListener, but still implements its interface
public:
    // A default NoOp listener for cases where no specific listener is provided
    struct NoOpListener : ExchangeListener {};

    // Default constructor uses a static NoOpListener
    Exchange() : m_listener(s_defaultNoOpListener) {}
    explicit Exchange(TListener& listener) : m_listener(listener) {}
    
    /**
     * @brief Places a limit buy order.
     * @return The ExchangeId assigned to the order if successful, otherwise std::nullopt.
     */
    OrderResult placeBuyOrder(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Price price,
        Quantity quantity,
        OrderIdStrView orderId = ""
    ) {
        return insertOrderInternal(sessionId, instrument, price, quantity, Order::Side::BUY, orderId);
    }

    /** @brief Places a market buy order at the highest possible execution priority. */
    OrderResult placeMarketBuyOrder(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Quantity quantity,
        OrderIdStrView orderId = ""
    ) {
        return placeBuyOrder(sessionId, instrument, Price(orderbook::kMarketBuyPrice), quantity, orderId);
    }
    
    /** @brief Places a limit sell order. */
    OrderResult placeSellOrder(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Price price,
        Quantity quantity,
        OrderIdStrView orderId = ""
    ) {
        return insertOrderInternal(sessionId, instrument, price, quantity, Order::Side::SELL, orderId);
    }
    
    /** @brief Places a market sell order at the highest possible execution priority. */
    OrderResult placeMarketSellOrder(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Quantity quantity,
        OrderIdStrView orderId = ""
    ) {
        return placeSellOrder(sessionId, instrument, orderbook::Price(orderbook::kMarketSellPrice), quantity, orderId);
    }
    
    /**
     * @brief Updates or creates a two-sided quote (bid/ask) for a session.
     */
    void quote(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Price bidPrice,
        Quantity bidQuantity,
        Price askPrice,
        Quantity askQuantity,
        QuoteIdView quoteId
    ) {
        auto book = m_books.getOrCreate(instrument, *this);
        auto orders = book->getQuotes(sessionId, quoteId, [&]() { return QuoteOrders{}; });
        book->quote(orders, bidPrice, bidQuantity, askPrice, askQuantity);
    }
    
    /**
     * @brief Cancels an existing order.
     * @param exchangeId The ID returned by the initial placement.
     * @param sessionId Validation session ID to ensure ownership.
     * @return True if cancellation was successful.
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
    
    /** @brief Returns a snapshot of the order book for the given instrument. */
    std::optional<Book> getBook(InstrumentSymbolView instrument) const {
        auto book = m_books.getOrderBook(instrument);
        if (!book) return std::nullopt;
        return book->getBook();
    }
    
    /** @brief Retrieves a copy of an order's current state. */
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
    
    /** @internal Implementation of OrderBookListener interface */
    void onOrder(const Order& order) {
        m_listener.onOrder(order);
    }
    
    /** @internal Implementation of OrderBookListener interface */
    void onTrade(const Trade& trade) {
        m_listener.onTrade(trade);
    }
    
    /** @brief Returns a synchronization guard for external transactional operations. */
    Guard lock() {
        return Guard(m_mu);
    }
    
    std::vector<InstrumentSymbol> instruments() {
        return m_books.instruments();
    }
    
    std::vector<std::shared_ptr<const Order>> orders() {
        return m_allOrders.all();
    }
    
private:
    BookMap<Exchange<TListener>> m_books; 
    OrderMap m_allOrders;
    SpinLock m_mu;
    
    ExchangeId nextId() {
        static std::atomic<ExchangeId> s_nextId{1};
        return s_nextId.fetch_add(1, std::memory_order_relaxed);
    }
    
    OrderResult insertOrderInternal(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Price price,
        Quantity quantity,
        Order::Side side,
        OrderIdStrView orderId
    ) {
        auto id = nextId();
        auto order = Order::create(sessionId, orderId, instrument, price, quantity, side, id);
        if (!order) {
            return std::unexpected(ErrorContext(InsertError::NullOrder, EngineConstants::kOrderCannotBeNull));
        }
        m_allOrders.add(order);
        auto book = m_books.getOrCreate(instrument, *this);
        return book->insertOrder(order);
    }
    
    TListener& m_listener; // Reference to the external listener

private:
    static inline NoOpListener s_defaultNoOpListener; // Static instance for default construction
};
} // namespace orderbook