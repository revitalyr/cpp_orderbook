/**
 * @file exchange.h
 * @brief High-level Exchange interface coordinating multiple OrderBooks and global order tracking.
 */
#pragma once

#include <string>
#include <optional>
#include <ranges>
#include <memory>
#include <vector>

#include "order.h"
#include "orderbook.h"
#include "bookmap.h"
#include "spinlock.h"
#include "ordermap.h"
#include "semantic_types.h"
#include "constants.h"
#include "engine_constants.h"

/**
 * @brief Interface for receiving asynchronous trade and order events from the Exchange.
 */
struct ExchangeListener {
    /** @brief Triggered whenever an order state changes (insertion, cancellation, etc.) */
    virtual void onOrder(const Order& ) {}
    /** @brief Triggered when a match occurs between a bid and an ask */
    virtual void onTrade(const Trade& ) {}
};

/** @brief Global default listener that performs no actions. */
inline ExchangeListener g_dummyListener;

using OrderResult = std::optional<ExchangeId>;
using CancelResult = bool;

/**
 * @class Exchange
 * @brief The primary entry point for the trading engine.
 * 
 * Manages a collection of OrderBooks keyed by instrument and maintains a global 
 * map of all active orders for efficient O(1) lookups and cancellations.
 */
template <typename TListener>
class Exchange : OrderBookListener { // Exchange still needs to implement OrderBookListener to be passed to OrderBook
public:
    Exchange() : m_listener(g_dummyListener) {}
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
    );
    
    /** @brief Places a market buy order at the highest possible execution priority. */
    OrderResult placeMarketBuyOrder(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Quantity quantity,
        OrderIdStrView orderId = ""
    ) {
        return placeBuyOrder(sessionId, instrument, Price(kMarketBuyPrice), quantity, orderId);
    }
    
    /** @brief Places a limit sell order. */
    OrderResult placeSellOrder(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Price price,
        Quantity quantity,
        OrderIdStrView orderId = ""
    );
    
    /** @brief Places a market sell order at the highest possible execution priority. */
    OrderResult placeMarketSellOrder(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Quantity quantity,
        OrderIdStrView orderId = ""
    ) {
        return placeSellOrder(sessionId, instrument, Price(kMarketSellPrice), quantity, orderId);
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
    );
    
    /**
     * @brief Cancels an existing order.
     * @param exchangeId The ID returned by the initial placement.
     * @param sessionId Validation session ID to ensure ownership.
     * @return True if cancellation was successful.
     */
    CancelResult cancelOrder(ExchangeId exchangeId, SessionIdView sessionId);
    
    /** @brief Returns a snapshot of the order book for the given instrument. */
    std::optional<Book> getBook(InstrumentSymbolView instrument) const;
    
    /** @brief Retrieves a copy of an order's current state. */
    std::optional<Order> getOrder(ExchangeId exchangeId) const;
    
    /** @brief Range-based view of all tracked orders. */
    auto getAllOrders() const {
        return m_allOrders.all() | std::views::transform([](const std::shared_ptr<const Order>& order) { return order; });
    }
    
    /** @brief Range-based view of all active instrument symbols. */
    auto getInstruments() const {
        return m_books.instruments() | std::views::transform([](const InstrumentSymbol& instrument) { return instrument; });
    }
    
    /** @internal Implementation of OrderBookListener interface */
    void onOrder(const Order& order) override {
        m_listener.onOrder(order);
    }
    
    /** @internal Implementation of OrderBookListener interface */
    void onTrade(const Trade& trade) override {
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
    BookMap<Exchange<TListener>> m_books; // BookMap needs to be templated on the type of listener OrderBook expects
    OrderMap m_allOrders;
    SpinLock m_mu;
    
    ExchangeId nextId();
    
    OrderResult insertOrderInternal(
        SessionIdView sessionId,
        InstrumentSymbolView instrument,
        Price price,
        Quantity quantity,
        Order::Side side,
        OrderIdStrView orderId
    );
    
    TListener& m_listener; // Reference to the external listener
};