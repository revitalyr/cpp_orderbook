#include <catch2/catch_test_macros.hpp>

#include "core/test.h"

using namespace orderbook;

TEST_CASE("OrderList basic operations", "[orderlist]") { // Renamed to camelCase
    OrderList orderList(100); // Renamed to camelCase
    REQUIRE(orderList.begin() == orderList.end()); // Renamed to camelCase

    auto order = TestOrder::create(ExchangeId(1), Price(100), Quantity(10), Order::Side::BUY); // Renamed to camelCase
    orderList.pushBack(order);

    REQUIRE(orderList.begin() != orderList.end()); // Renamed to camelCase
    REQUIRE(*(orderList.begin()) == order); // Renamed to camelCase
}

TEST_CASE("OrderList iterator", "[orderlist]") { // Renamed to camelCase
    OrderList orderList(100); // Renamed to camelCase
    REQUIRE(orderList.begin() == orderList.end()); // Renamed to camelCase

    auto order = TestOrder::create(1, 100, 10, Order::Side::BUY); // Renamed to camelCase
    orderList.pushBack(order); // Renamed to camelCase

    REQUIRE(orderList.begin() != orderList.end()); // Renamed to camelCase
    REQUIRE(*(orderList.begin()) == order); // Renamed to camelCase

    auto order2 = TestOrder::create(2, 100, 10, Order::Side::BUY); // Renamed to camelCase
    orderList.pushBack(order2); // Renamed to camelCase
    
    REQUIRE(*(orderList.begin()) == order); // Renamed to camelCase

    auto it = orderList.begin(); // Renamed to camelCase
    ++it;
    REQUIRE(*it == order2); // Renamed to camelCase
    ++it;
    REQUIRE(it == orderList.end()); // Renamed to camelCase
}
