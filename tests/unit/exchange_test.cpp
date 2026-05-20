#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string> // For std::string
#include <vector>
#include <optional>

#include "safety/production_safety.h" // Explicitly include ProductionSafety
import orderbook;

using namespace orderbook;

struct TestListener : ExchangeListener {
    std::vector<Trade> m_trades;
    void onTrade(const Trade& trade) override {
        m_trades.push_back(trade);
    }
    std::vector<Order> m_orders;
    void onOrder(const Order& order) override {
        m_orders.push_back(order);
    }
};

TEST_CASE("Exchange insert order buy", "[exchange]") {
    // Disable production safety in tests to prevent recursion with Boost Test Framework
    ::ProductionSafety::enableSafety(false);
    
    TestExchange<Exchange::NoOpListener> exchange;

    auto order1Id = exchange.placeBuyOrder(1.0, 10, "1");
    auto order2Id = exchange.placeBuyOrder(2.0, 10, "2");

    REQUIRE(exchange.bidCount() == 2);
    REQUIRE(exchange.askCount() == 0);

    REQUIRE(exchange.bidIndex(order2Id.value()) == 0);
    REQUIRE(exchange.bidIndex(order1Id.value()) == 1);
}

TEST_CASE("Exchange insert order buy 2", "[exchange]") {
    TestExchange<Exchange::NoOpListener> exchange; // Use default listener

    auto order2Id = exchange.placeBuyOrder(2.0, 10, "2");
    auto order1Id = exchange.placeBuyOrder(1.0, 10, "1");

    // should end up with same ordering

    REQUIRE(exchange.bidCount() == 2);
    REQUIRE(exchange.askCount() == 0);

    REQUIRE(exchange.bidIndex(order2Id.value()) == 0);
    REQUIRE(exchange.bidIndex(order1Id.value()) == 1);
}

TEST_CASE("Exchange insert order sell", "[exchange]") {
    TestExchange<Exchange::NoOpListener> exchange; // Use default listener

    auto order1Id = exchange.placeSellOrder(1.0, 10, "1");
    auto order2Id = exchange.placeSellOrder(2.0, 10, "2");

    REQUIRE(exchange.bidCount() == 0);
    REQUIRE(exchange.askCount() == 2);

    REQUIRE(exchange.askIndex(order2Id.value()) == 1);
    REQUIRE(exchange.askIndex(order1Id.value()) == 0);
}

TEST_CASE("Exchange insert order sell 2", "[exchange]") {
    TestExchange<Exchange::NoOpListener> exchange; // Use default listener

    auto order2Id = exchange.placeSellOrder(2.0, 10, "2");
    auto order1Id = exchange.placeSellOrder(1.0, 10, "1");

    REQUIRE(exchange.bidCount() == 0);
    REQUIRE(exchange.askCount() == 2);

    REQUIRE(exchange.askIndex(order2Id.value()) == 1);
    REQUIRE(exchange.askIndex(order1Id.value()) == 0);
}

TEST_CASE("Exchange insert order buy same price", "[exchange]") {
    TestExchange<Exchange::NoOpListener> exchange; // Use default listener

    auto order1Id = exchange.placeBuyOrder(1.0, 10, "1");
    auto order2Id = exchange.placeBuyOrder(2.0, 10, "2");
    auto order3Id = exchange.placeBuyOrder(2.0, 25, "3");

    REQUIRE(exchange.bidCount() == 2);
    REQUIRE(exchange.getBook(kDefaultInstrument).value().m_bidOrderIds.size() == 3);
    REQUIRE(exchange.askCount() == 0);
    
    REQUIRE(exchange.bidIndex(order2Id.value()) == 0);
    REQUIRE(exchange.bidIndex(order3Id.value()) == 1);
    REQUIRE(exchange.bidIndex(order1Id.value()) == 2);
}

TEST_CASE("Exchange fill order", "[exchange]") {
    TestListener testListener;
    
    TestExchange<TestListener> exchange(testListener);

    auto order1Id = exchange.placeBuyOrder(1.0, 10, "1");

    REQUIRE(testListener.m_trades.size() == 0);

    auto order2Id = exchange.placeSellOrder(0.75, 10, "2");

    REQUIRE(testListener.m_trades.size() == 1);
    REQUIRE(testListener.m_trades[0].m_aggressor.m_exchangeId == order2Id.value());
    REQUIRE(testListener.m_trades[0].m_opposite.m_exchangeId == order1Id.value());
}

TEST_CASE("Exchange partial fill", "[exchange]") {

    TestListener testListener;
    TestExchange<TestListener> exchange(testListener);

    auto order1Id = exchange.placeBuyOrder(1.0, 20, "1");

    REQUIRE(testListener.m_trades.size() == 0);

    auto order2Id = exchange.placeSellOrder(.75, 10, "2");

    REQUIRE(testListener.m_trades.size() == 1);
    REQUIRE(testListener.m_trades[0].m_aggressor.m_exchangeId == order2Id.value());
    REQUIRE(testListener.m_trades[0].m_opposite.m_exchangeId == order1Id.value());

    REQUIRE(exchange.getOrder(order2Id.value()).value().remainingQuantity() == 0);
    REQUIRE(exchange.getOrder(order1Id.value()).value().remainingQuantity() == 10);

    REQUIRE(exchange.bidCount() == 1);
    REQUIRE(exchange.askCount() == 0);

}

TEST_CASE("Exchange cancel order", "[exchange]") {

    TestListener testListener;
    
    TestExchange<TestListener> exchange(testListener);

    auto order1Id = exchange.placeBuyOrder(1.0, 20, "1");
    REQUIRE(exchange.cancelOrder(order1Id.value(), "session"));
    REQUIRE_FALSE(exchange.cancelOrder(order1Id.value(), "session"));

    // should be an event for the order and the cancel
    REQUIRE(testListener.m_orders.size() == 2);
    REQUIRE(exchange.bidCount() == 0);
}

TEST_CASE("Exchange cancel invalid", "[exchange]") {

    TestListener testListener;
    TestExchange<TestListener> exchange(testListener);

    auto order1Id = exchange.placeBuyOrder(1.0, 20, "1");
    REQUIRE_THROWS(exchange.cancelOrder(order1Id.value() + 1, "dummy"));
}

TEST_CASE("Exchange market buy", "[exchange]") {

    TestListener testListener;
    
    TestExchange<TestListener> exchange(testListener);

    exchange.placeSellOrder(1.0, 20, "1");
    exchange.placeMarketBuyOrder(10, "2");

    REQUIRE(testListener.m_orders.size() == 4);
    REQUIRE(testListener.m_trades.size() == 1);
    REQUIRE(exchange.bidCount() == 0);
    REQUIRE(exchange.askCount() == 1);
}

TEST_CASE("Exchange market buy cancel remaining", "[exchange]") {

    TestListener testListener;
    
    TestExchange<TestListener> exchange(testListener);

    exchange.placeSellOrder(1.0, 20, "1");
    exchange.placeMarketBuyOrder(30, "2");

    REQUIRE(testListener.m_orders.size() == 5);
    REQUIRE(testListener.m_trades.size() == 1);
    REQUIRE(exchange.bidCount() == 0);
    REQUIRE(exchange.askCount() == 0);
}

TEST_CASE("Exchange market buy multi level", "[exchange]") {

    TestListener testListener;
    
    TestExchange<TestListener> exchange(testListener);

    exchange.placeSellOrder(1.0, 20, "1");
    exchange.placeSellOrder(2.0, 20, "2");

    exchange.placeMarketBuyOrder(30, "3");

    // initial orders (3) + 2x2 updates due to trades (1 full and 1 partial) = 7 statuses
    REQUIRE(testListener.m_orders.size() == 7);
    REQUIRE(testListener.m_trades.size() == 2);
    REQUIRE(exchange.bidCount() == 0);
    REQUIRE(exchange.askCount() == 1);
    REQUIRE_FALSE(exchange.getBook(kDefaultInstrument).value().m_asks.empty());
    REQUIRE(exchange.getBook(kDefaultInstrument).value().m_asks[0].m_quantity == 10);
}

TEST_CASE("Exchange market buy one sided", "[exchange]") {

    TestListener testListener;
    
    TestExchange<TestListener> exchange(testListener);

    exchange.placeMarketBuyOrder(30, "1");

    REQUIRE(testListener.m_orders.size() == 2);
    REQUIRE(testListener.m_trades.size() == 0);
    REQUIRE(exchange.bidCount() == 0);
    REQUIRE(exchange.askCount() == 0);
}

TEST_CASE("Exchange order immutability", "[exchange]") {

    TestListener testListener;
    TestExchange<TestListener> exchange(testListener);

    auto order1Id = exchange.placeBuyOrder(1.0, 30, "1");
    Order order = exchange.getOrder(order1Id.value()).value();
    exchange.placeSellOrder(1.0, 10, "2");
    Order latest = exchange.getOrder(order1Id.value()).value();

    REQUIRE(order.remainingQuantity() == 30);
    REQUIRE(latest.remainingQuantity() == 20);
}
