#pragma once

#include <algorithm> // For std::find, std::distance
#include <string>    // For std::string

import orderbook;

namespace orderbook {

// C++26: Modern test utilities with smart pointers
template <typename TListener>
class TestExchange : public Exchange<TListener> {
public:
    explicit TestExchange(TListener& listener) : Exchange<TListener>(listener) {}
    
    // Simplified API for testing with default instrument (using OrderResult from module) // Renamed to camelCase
    OrderInsertResult placeBuyOrder(SessionIdView sessionId, Price price, Quantity quantity, OrderIdStrView orderId = "") {
        return Exchange<TListener>::placeBuyOrder(sessionId, orderbook::kDefaultInstrument, price, quantity, orderId);
    }
    
    OrderInsertResult placeSellOrder(SessionIdView sessionId, Price price, Quantity quantity, OrderIdStrView orderId = "") {
        return Exchange<TListener>::placeSellOrder(sessionId, orderbook::kDefaultInstrument, price, quantity, orderId);
    }
    
    OrderInsertResult placeMarketBuyOrder(SessionIdView sessionId, Quantity quantity, OrderIdStrView orderId = "") {
        return placeBuyOrder(sessionId, Price(orderbook::kMarketBuyPrice), quantity, orderId);
    }
    
    OrderInsertResult placeMarketSellOrder(SessionIdView sessionId, Quantity quantity, OrderIdStrView orderId = "") {
        return Exchange<TListener>::placeMarketSellOrder(sessionId, orderbook::kDefaultInstrument, quantity, orderId);
    }

    // API Overloads for legacy test compatibility
    OrderInsertResult placeBuyOrder(Price price, Quantity quantity, OrderIdStrView orderId = "") {
        return placeBuyOrder(orderbook::EngineConstants::kTestSessionId, price, quantity, orderId);
    }
    OrderInsertResult placeSellOrder(Price price, Quantity quantity, OrderIdStrView orderId = "") {
        return placeSellOrder(orderbook::EngineConstants::kTestSessionId, price, quantity, orderId);
    }
    OrderInsertResult placeMarketBuyOrder(Quantity quantity, OrderIdStrView orderId = "") {
        return placeMarketBuyOrder(orderbook::EngineConstants::kTestSessionId, quantity, orderId);
    }
    OrderInsertResult placeMarketSellOrder(Quantity quantity, OrderIdStrView orderId = "") {
        return placeMarketSellOrder(orderbook::EngineConstants::kTestSessionId, quantity, orderId);
    }

    // Verification Helpers
    ObjectCount bidCount() const { return Exchange<TListener>::getBook(orderbook::kDefaultInstrument).value().m_bids.size(); }
    ObjectCount askCount() const { return Exchange<TListener>::getBook(orderbook::kDefaultInstrument).value().m_asks.size(); }
    
    int64_t bidIndex(ExchangeId id) const {
        auto ids = Exchange<TListener>::getBook(orderbook::kDefaultInstrument).value().m_bidOrderIds;
        auto it = std::find(ids.begin(), ids.end(), id);
        return it == ids.end() ? -1 : static_cast<int>(std::distance(ids.begin(), it));
    }
    
    int64_t askIndex(ExchangeId id) const {
        auto ids = Exchange<TListener>::getBook(kDefaultInstrument).value().m_askOrderIds;
        auto it = std::find(ids.begin(), ids.end(), id);
        return it == ids.end() ? -1 : static_cast<int>(std::distance(ids.begin(), it));
    }

    // C++26: Modern range-based queries
    auto getOrdersBySide(Order::Side side) const {
        return getAllOrders() 
            | std::views::filter([side](const auto& order) { return order->m_side == side; }); // Renamed to m_snake_case
    }
    
    auto getOrdersBySession(SessionIdView sessionId) const {
        return getAllOrders()
            | std::views::filter([sessionId](const auto& order) { return order->sessionId() == sessionId; }); // Renamed to camelCase
    }
};

// C++26: Modern TestOrder with smart pointers and factory methods
class TestOrder : public Order {
public:
    // C++26: Factory methods returning smart pointers
    static std::shared_ptr<TestOrder> createOrder(ExchangeId id, Price price, Quantity quantity, Order::Side side) {
        using Allocator = MemoryPoolAllocator<TestOrder, OrderPool>;
        return std::allocate_shared<TestOrder>(Allocator{}, id, price, quantity, side);
    }
    
    static std::shared_ptr<TestOrder> create(
        SessionIdView sessionId,
        OrderIdStrView orderId,
        Price price,
        Quantity quantity,
        Order::Side side,
        ExchangeId exchange_id
    ) {
        using Allocator = MemoryPoolAllocator<TestOrder, OrderPool>;
        return std::allocate_shared<TestOrder>(Allocator{}, sessionId, orderId, orderbook::kDefaultInstrument, price, quantity, side, exchange_id);
    }

    // Factory method for legacy benchmarks (4 arguments)
    static std::shared_ptr<TestOrder> create(ExchangeId id, Price price, Quantity quantity, Order::Side side) {
        using Allocator = MemoryPoolAllocator<TestOrder, OrderPool>;
        return std::allocate_shared<TestOrder>(Allocator{}, orderbook::EngineConstants::kTestSessionId, "", orderbook::kDefaultInstrument, price, quantity, side, id);
    }
    
    // Legacy constructors for compatibility
    TestOrder(ExchangeId id, Price price, Quantity quantity, Order::Side side) // Renamed to camelCase
        : Order(orderbook::EngineConstants::kTestSessionId, "", orderbook::kDefaultInstrument, price, quantity, side, id) {
            // Avoid std::to_string heap allocation; Order constructor handles numeric ID if string is empty
            this->m_orderIdNum = id; 
        }
    
    TestOrder( // Renamed to camelCase
        OrderIdStrView orderId,
        ExchangeId id,
        Price price,
        Quantity quantity,
        Order::Side side
    ) : Order(orderbook::EngineConstants::kTestSessionId, orderId, orderbook::kDefaultInstrument, price, quantity, side, id) {}
    
    TestOrder( // Renamed to camelCase
        SessionIdView sessionId,
        OrderIdStrView orderId,
        Price price,
        Quantity quantity,
        Order::Side side,
        ExchangeId exchange_id
    ) : Order(sessionId, orderId, orderbook::kDefaultInstrument, price, quantity, side, exchange_id) {}
};

// C++26: Modern test utilities
namespace TestUtils {
    // Create a test order book with smart pointers
    template <typename TListener>
    inline std::unique_ptr<OrderBook<TListener>> createTestOrderBook(TListener& listener) { // Renamed to camelCase
        return std::make_unique<OrderBook<TListener>>(orderbook::kDefaultInstrument, listener);
    }
    
    // Helper to validate order book state
    inline bool validateOrderBook(const OrderBook& book) {
        auto bookSnapshot = book.getBook();
        return !bookSnapshot.m_bids.empty() || !bookSnapshot.m_asks.empty(); // Renamed to m_snake_case
    }
    
    // C++26: Range-based order validation
    inline auto validateOrders(std::ranges::range auto orders) {
        return std::ranges::all_of(orders, [](const auto& order) {
            return order && order->remainingQuantity() > 0; // Renamed to camelCase
        });
    }
}

// C++20: Constants for testing
inline const std::string kDummyInstrument = orderbook::kDefaultInstrument; // Renamed to kPascalCase

} // namespace orderbook
