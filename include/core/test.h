#pragma once

#include <algorithm> // For std::find, std::distance
#include <string>    // For std::string

#include "exchange.h"
#include "order.h"
#include "memory_pool.h"

namespace orderbook {

inline constexpr const char* kDefaultInstrument = "SYM1";

namespace EngineConstants {
inline constexpr std::string_view kTestSessionId = "session";
inline constexpr std::string_view kTestOrderIdPrefix = "order_";
inline constexpr std::string_view kTestQuoteId = "quote1";
inline constexpr std::string_view kTestInstrumentAAPL = "AAPL";
inline constexpr std::string_view kTestInstrumentGOOG = "GOOG";
inline constexpr std::string_view kTestInstrumentMSFT = "MSFT";
}

// C++26: Modern test utilities with smart pointers
template <typename TListener>
class TestExchange : public Exchange<TListener> {
public:
    TestExchange() = default;
    explicit TestExchange(TListener& listener) : Exchange<TListener>(listener) {}
    
    // Simplified API for testing with default instrument
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

    // Full API for testing with explicit instrument
    OrderInsertResult placeBuyOrder(SessionIdView sessionId, InstrumentSymbolView instrument, Price price, Quantity quantity, OrderIdStrView orderId = "") {
        return Exchange<TListener>::placeBuyOrder(sessionId, instrument, price, quantity, orderId);
    }
    
    OrderInsertResult placeSellOrder(SessionIdView sessionId, InstrumentSymbolView instrument, Price price, Quantity quantity, OrderIdStrView orderId = "") {
        return Exchange<TListener>::placeSellOrder(sessionId, instrument, price, quantity, orderId);
    }
    
    OrderInsertResult placeMarketBuyOrder(SessionIdView sessionId, InstrumentSymbolView instrument, Quantity quantity, OrderIdStrView orderId = "") {
        return placeBuyOrder(sessionId, instrument, Price(orderbook::kMarketBuyPrice), quantity, orderId);
    }
    
    OrderInsertResult placeMarketSellOrder(SessionIdView sessionId, InstrumentSymbolView instrument, Quantity quantity, OrderIdStrView orderId = "") {
        return Exchange<TListener>::placeMarketSellOrder(sessionId, instrument, quantity, orderId);
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
        return this->getAllOrders() 
            | std::views::filter([side](const auto& order) { return order->m_side == side; });
    }
    
    auto getOrdersBySession(SessionIdView sessionId) const {
        return this->getAllOrders()
            | std::views::filter([sessionId](const auto& order) { return order->sessionId() == sessionId; });
    }
};

// C++26: Modern TestOrder with smart pointers and factory methods
class TestOrder : public Order {
public:
    TestOrder(ExchangeId id, Price price, Quantity quantity, Order::Side side)
        : Order(EngineConstants::kTestSessionId, "", kDefaultInstrument, price, quantity, side, id) {
        m_orderIdNum = id;
    }

    TestOrder(SessionIdView sessionId, OrderIdStrView orderId,
              Price price, Quantity quantity, Order::Side side, ExchangeId exchangeId)
        : Order(sessionId, orderId, kDefaultInstrument, price, quantity, side, exchangeId) {}

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
        return std::allocate_shared<TestOrder>(Allocator{}, sessionId, orderId, price, quantity, side, exchange_id);
    }

    // Overload for tests that don't specify orderId
    static std::shared_ptr<TestOrder> create(
        SessionIdView sessionId,
        Price price,
        Quantity quantity,
        Order::Side side,
        ExchangeId exchange_id
    ) {
        using Allocator = MemoryPoolAllocator<TestOrder, OrderPool>;
        return std::allocate_shared<TestOrder>(Allocator{}, sessionId, "", price, quantity, side, exchange_id);
    }
    
    // Overload for tests that specify ExchangeId but use default session and empty orderId
    static std::shared_ptr<TestOrder> create(
        ExchangeId exchange_id,
        Price price,
        Quantity quantity,
        Order::Side side
    ) {
        using Allocator = MemoryPoolAllocator<TestOrder, OrderPool>;
        return std::allocate_shared<TestOrder>(Allocator{}, EngineConstants::kTestSessionId, "", price, quantity, side, exchange_id);
    }

    // Overload for tests that use default session and exchange
    static std::shared_ptr<TestOrder> create(
        Price price,
        int quantity,
        Order::Side side
    ) {
        using Allocator = MemoryPoolAllocator<TestOrder, OrderPool>;
        return std::allocate_shared<TestOrder>(Allocator{}, EngineConstants::kTestSessionId, "", price, Quantity(quantity), side, ExchangeId(1));
    }
};

// C++26: Modern test utilities
namespace TestUtils {
    // Create a test order book with smart pointers
    template <typename TListener>
    inline std::unique_ptr<OrderBook<TListener>> createTestOrderBook(TListener& listener) {
        return std::make_unique<OrderBook<TListener>>(orderbook::kDefaultInstrument, listener);
    }
    
    // Helper to validate order book state
    template <typename TListener>
    inline bool validateOrderBook(const OrderBook<TListener>& book) {
        auto bookSnapshot = book.getBook();
        return !bookSnapshot.m_bids.empty() || !bookSnapshot.m_asks.empty();
    }
    
    // C++26: Range-based order validation
    inline auto validateOrders(std::ranges::range auto orders) {
        return std::ranges::all_of(orders, [](const auto& order) {
            return order && order->remainingQuantity() > 0;
        });
    }
}

// C++20: Constants for testing
inline const std::string kDummyInstrument = orderbook::kDefaultInstrument;

} // namespace orderbook
