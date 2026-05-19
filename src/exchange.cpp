#include "core/exchange.h"
#include "core/orderbook.h"
#include "core/insert_result.h"
#include <stdexcept>
#include <string>
#include <string_view>
#include <iostream>

#define LOCK_EXCHANGE_MUTEX() std::lock_guard<std::mutex> lock(m_mu)

std::optional<Order> Exchange::getOrder(ExchangeId exchangeId) const {
    auto order = m_allOrders.get(exchangeId);
    if (!order) return std::nullopt;
    
    auto orderBook = m_books.getOrderBook(order->instrument());
    if (!orderBook) return std::nullopt;
    
    auto bookGuard = orderBook->lock();
    return std::optional<Order>(orderBook->getOrder(order));
}

std::optional<Book> Exchange::getBook(std::string_view instrument) const {
    auto orderBook = m_books.getOrderBook(instrument);
    if (!orderBook) return std::nullopt;
    
    auto bookGuard = orderBook->lock();
    return orderBook->getBook();
}

CancelResult Exchange::cancelOrder(ExchangeId exchangeId, SessionIdView sessionId) {
    auto order = m_allOrders.get(exchangeId);
    if (!order) {
        throw std::invalid_argument(std::string(::EngineConstants::kOrderNotFound));
    }
    
    if (order->sessionId() != sessionId) {
        return false;
    }
    
    auto orderBook = m_books.getOrderBook(order->instrument());
    if (!orderBook) {
        return false;
    }

    auto bookGuard = orderBook->lock();
    auto result = orderBook->cancelOrder(order);
    return result == 0;
}

OrderResult Exchange::insertOrderInternal(
    SessionIdView sessionId,
    InstrumentSymbolView instrument,
    Price price,
    Quantity quantity,
    Order::Side side,
    OrderIdStrView orderId
) {
    try {
        auto orderBook = m_books.getOrCreate(instrument, *this);
        if (!orderBook) {
            return std::nullopt;
        }
        
        auto bookGuard = orderBook->lock();
        ExchangeId id = nextId();
        
        auto order = Order::create(
            sessionId,
            orderId,
            orderBook->m_instrument,
            price,
            quantity,
            side,
            id
        );
        
        m_allOrders.add(order);
        
        // C++20: Use new insertOrder with explicit result checking
        auto result = orderBook->insertOrder(order);
        
        // C++23 style result checking
        if (!result.has_value()) {
            const auto& error = result.error();
            
            // Log error details
            std::cerr << ::EngineConstants::kExchangeErrorPrefix << ::EngineConstants::kOrderInsertionFailed << error.toString() << "\n";
            
            // Handle specific error types with configurable strategies
            switch (error.code) {
                case InsertError::StackOverflowProtection:
                    // Critical: Remove order from allOrders to prevent orphaned orders
                    m_allOrders.remove(order->m_exchangeId);
                    // Could implement retry with backoff here
                    return std::nullopt;
                    
                case InsertError::OrderAlreadyExists:
                    // Order already exists - this is idempotent, return existing ID
                    return id;
                    
                case InsertError::NullOrder:
                case InsertError::InvalidQuantity:
                    // Validation errors - remove from allOrders
                    m_allOrders.remove(order->m_exchangeId);
                    return std::nullopt;
                    
                default:
                    // Unknown error - safe fallback
                    m_allOrders.remove(order->m_exchangeId);
                    return std::nullopt;
            }
        }
        
        return id;
    } catch (const std::exception& e) {
        std::cerr << ::EngineConstants::kExchangeErrorPrefix << ::EngineConstants::kExceptionInInsertOrder << e.what() << "\n";
        return std::nullopt;
    }
}

OrderResult Exchange::placeBuyOrder(
    SessionIdView sessionId,
    InstrumentSymbolView instrument,
    Price price,
    Quantity quantity,
    OrderIdStrView orderId
) {
    return insertOrderInternal(sessionId, instrument, price, quantity, Order::Side::BUY, orderId);
}

OrderResult Exchange::placeSellOrder(
    SessionIdView sessionId,
    InstrumentSymbolView instrument,
    Price price,
    Quantity quantity,
    OrderIdStrView orderId
) {
    return insertOrderInternal(sessionId, instrument, price, quantity, Order::Side::SELL, orderId);
}

void Exchange::quote(
    SessionIdView sessionId,
    InstrumentSymbolView instrument,
    Price bidPrice,
    Quantity bidQuantity,
    Price askPrice,
    Quantity askQuantity,
    QuoteIdView quoteId
) {
    auto orderBook = m_books.getOrCreate(instrument, *this);
    auto lock = orderBook->lock();
    
    auto orders = orderBook->getQuotes(sessionId, quoteId,
        [&]() -> QuoteOrders {
            QuoteOrders result;
            if (bidQuantity > Quantity(0)) {
                result.m_bid = Order::create(sessionId, quoteId, orderBook->m_instrument, bidPrice, bidQuantity, Order::Side::BUY, nextId());
                m_allOrders.add(result.m_bid);
            }
            
            if (askQuantity > Quantity(0)) {
                result.m_ask = Order::create(
                    sessionId,
                    quoteId,
                    orderBook->m_instrument,
                    askPrice,
                    askQuantity,
                    Order::Side::SELL,
                    nextId()
                );
                m_allOrders.add(result.m_ask);
            }
            
            return result;
        }
    );
    
    orderBook->quote(orders, bidPrice, bidQuantity, askPrice, askQuantity);
}

ExchangeId Exchange::nextId() {
    static std::atomic<ExchangeId> id = ExchangeId(0);
    return ++id;
}
