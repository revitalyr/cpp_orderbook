#include <catch2/catch_test_macros.hpp>
#include <string> // For std::string
#include <vector> // For std::vector

#include "core/orderbook.h"
#include "core/order.h"
#include "core/exchange.h"
#include "core/test.h"

using namespace orderbook;

struct TestOrderBookListener : ExchangeListener {
    // Implement onOrder/onTrade if needed for specific tests
};

TEST_CASE("OrderBook cancel order", "[orderbook]") {
    TestOrderBookListener testListener;
    OrderBook<TestOrderBookListener> orderBook(orderbook::kDummyInstrument, testListener);

    auto order1 = TestOrder::createOrder(orderbook::ExchangeId(1), orderbook::Price(100), orderbook::Quantity(10), orderbook::Order::Side::BUY);
    orderBook.insertOrder(order1);
    orderBook.cancelOrder(order1);

    auto bookLevels = orderBook.getBook();
    REQUIRE(bookLevels.m_bids.size() == 0);

    auto order2 = TestOrder::createOrder(orderbook::ExchangeId(1), orderbook::Price(100), orderbook::Quantity(10), orderbook::Order::Side::BUY);
    orderBook.insertOrder(order2);
    auto order3 = TestOrder::createOrder(orderbook::ExchangeId(2), orderbook::Price(90), orderbook::Quantity(10), orderbook::Order::Side::BUY);
    orderBook.insertOrder(order3);
    auto order4 = TestOrder::createOrder(orderbook::ExchangeId(3), orderbook::Price(80), orderbook::Quantity(10), orderbook::Order::Side::BUY);
    orderBook.insertOrder(order4);

    orderBook.cancelOrder(order3);

    auto bookSnapshot = orderBook.getBook();

    REQUIRE(bookSnapshot.m_bids.size() == 2);
    REQUIRE(bookSnapshot.m_bids[0].m_price == 100);
    REQUIRE(bookSnapshot.m_bids[1].m_price == Price(80));
}

TEST_CASE("OrderBook book levels", "[orderbook]") {
    TestOrderBookListener testListener;
    OrderBook<TestOrderBookListener> orderBook(orderbook::kDummyInstrument, testListener);
    
    auto order1 = TestOrder::createOrder(orderbook::ExchangeId(1), orderbook::Price(100), orderbook::Quantity(10), orderbook::Order::Side::BUY);
    orderBook.insertOrder(order1);

    auto bookLevels = orderBook.getBook();
    
    REQUIRE_FALSE(bookLevels.m_bids.empty());
    REQUIRE(bookLevels.m_bids[0].m_price == orderbook::Price(100));
    REQUIRE(bookLevels.m_bids[0].m_quantity == orderbook::Quantity(10));
}

TEST_CASE("OrderBook book levels sum", "[orderbook]") {
    TestOrderBookListener testListener;
    OrderBook<TestOrderBookListener> orderBook(orderbook::kDummyInstrument, testListener);

    auto order1 = TestOrder::createOrder(orderbook::ExchangeId(1), orderbook::Price(100), orderbook::Quantity(10), orderbook::Order::Side::BUY);
    orderBook.insertOrder(order1);
    auto order2 = TestOrder::createOrder(orderbook::ExchangeId(2), orderbook::Price(100), orderbook::Quantity(10), orderbook::Order::Side::BUY);
    orderBook.insertOrder(order2);

    auto bookLevels = orderBook.getBook();

    REQUIRE_FALSE(bookLevels.m_bids.empty());
    REQUIRE(bookLevels.m_bids[0].m_price == orderbook::Price(100));
    REQUIRE(bookLevels.m_bids[0].m_quantity == orderbook::Quantity(20));
}

TEST_CASE("OrderBook book levels multiple", "[orderbook]") {
    TestOrderBookListener testListener;
    OrderBook<TestOrderBookListener> orderBook(orderbook::kDummyInstrument, testListener);

    auto order1 = TestOrder::createOrder(orderbook::ExchangeId(1), orderbook::Price(100), orderbook::Quantity(10), orderbook::Order::Side::BUY);
    orderBook.insertOrder(order1);
    auto order2 = TestOrder::createOrder(orderbook::ExchangeId(2), orderbook::Price(100), orderbook::Quantity(10), orderbook::Order::Side::BUY);
    orderBook.insertOrder(order2);
    auto order3 = TestOrder::createOrder(orderbook::ExchangeId(3), orderbook::Price(200), orderbook::Quantity(30), orderbook::Order::Side::BUY);
    orderBook.insertOrder(order3);

    auto bookLevels = orderBook.getBook();

    REQUIRE(bookLevels.m_bids.size() >= 2);
    REQUIRE(bookLevels.m_bids[0].m_price == orderbook::Price(200));
    REQUIRE(bookLevels.m_bids[0].m_quantity == orderbook::Quantity(30));
    REQUIRE(bookLevels.m_bids[1].m_price == orderbook::Price(100));
    REQUIRE(bookLevels.m_bids[1].m_quantity == orderbook::Quantity(20));
}

TEST_CASE("OrderBook book levels order", "[orderbook]") {
    TestOrderBookListener testListener;
    OrderBook<TestOrderBookListener> orderBook(orderbook::kDummyInstrument, testListener);

    // Store orders in a vector to keep them alive for the duration of the test
    std::vector<std::shared_ptr<Order>> orders;
    auto addOrder = [&](ExchangeId id, Price p, Quantity q, Order::Side s) {
        auto o = TestOrder::createOrder(id, p, q, s);
        orders.push_back(o);
        orderBook.insertOrder(o);
    };

    addOrder(ExchangeId(1), Price(100), Quantity(10), Order::Side::BUY);
    addOrder(ExchangeId(2), Price(101), Quantity(10), Order::Side::BUY);
    addOrder(ExchangeId(3), Price(99), Quantity(10), Order::Side::BUY);
    addOrder(ExchangeId(4), Price(98), Quantity(10), Order::Side::BUY);

    addOrder(ExchangeId(5), Price(200), Quantity(10), Order::Side::SELL);
    addOrder(ExchangeId(6), Price(199), Quantity(10), Order::Side::SELL);
    addOrder(ExchangeId(7), Price(201), Quantity(10), Order::Side::SELL);
    addOrder(ExchangeId(8), Price(202), Quantity(10), Order::Side::SELL);

    auto bookLevels = orderBook.getBook();
    
    REQUIRE(bookLevels.m_bids.size() >= 4);
    REQUIRE(bookLevels.m_bids[0].m_price == orderbook::Price(101));
    REQUIRE(bookLevels.m_bids[1].m_price == orderbook::Price(100));
    REQUIRE(bookLevels.m_bids[2].m_price == orderbook::Price(99));
    REQUIRE(bookLevels.m_bids[3].m_price == orderbook::Price(98));
    
    REQUIRE(bookLevels.m_asks.size() >= 4);
    REQUIRE(bookLevels.m_asks[0].m_price == orderbook::Price(199));
    REQUIRE(bookLevels.m_asks[1].m_price == orderbook::Price(200));
    REQUIRE(bookLevels.m_asks[2].m_price == orderbook::Price(201));
    REQUIRE(bookLevels.m_asks[3].m_price == orderbook::Price(202));
}

TEST_CASE("OrderBook session ID comparison", "[orderbook]") {
    const std::string session_1(orderbook::EngineConstants::kTestSessionId);
    const std::string session_2("session_2");
    const std::string session_3(orderbook::EngineConstants::kTestSessionId);

    REQUIRE(session_1 < session_2);
    REQUIRE_FALSE(session_2 < session_1);
    REQUIRE(session_1 == session_1);
    REQUIRE_FALSE(session_1 == session_2);
    REQUIRE(session_1 == session_3);
}

TEST_CASE("OrderBook SessionQuoteId comparison", "[orderbook]") {
    std::string session_1(orderbook::EngineConstants::kTestSessionId);
    std::string session_2("session_2");
    std::string session_3(orderbook::EngineConstants::kTestSessionId);

    SessionQuoteId session_quote_id_1(session_1, orderbook::EngineConstants::kTestQuoteId);
    SessionQuoteId session_quote_id_2(session_2, "quote_2");
    SessionQuoteId session_quote_id_3(session_3, orderbook::EngineConstants::kTestQuoteId);

    REQUIRE(session_quote_id_1 < session_quote_id_2);
    REQUIRE_FALSE(session_quote_id_2 < session_quote_id_1);
    REQUIRE(session_quote_id_1 == session_quote_id_1);
    REQUIRE_FALSE(session_quote_id_1 == session_quote_id_2);
    REQUIRE(session_quote_id_1 == session_quote_id_3);
}

TEST_CASE("OrderBook quoting functionality", "[orderbook]") {
}
