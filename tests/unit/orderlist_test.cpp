#include <catch2/catch_test_macros.hpp>

#include "core/orderbook.h"
#include "core/exchange.h" // For ExchangeListener
#include "core/test.h"

TEST_CASE("OrderList basic operations", "[orderlist]") {
    OrderList orderList(100);
    REQUIRE(orderList.begin() == orderList.end());

    auto order = TestOrder::create(1, 100, 10, Order::Side::BUY);
    orderList.pushBack(order);

    REQUIRE(orderList.begin() != orderList.end());
    REQUIRE(*(orderList.begin()) == order);
}

TEST_CASE("OrderList iterator", "[orderlist]") {
    OrderList orderList(100);
    REQUIRE(orderList.begin() == orderList.end());

    auto order = TestOrder::create(1, 100, 10, Order::Side::BUY);
    orderList.pushBack(order);

    REQUIRE(orderList.begin() != orderList.end());
    REQUIRE(*(orderList.begin()) == order);

    auto order2 = TestOrder::create(2, 100, 10, Order::Side::BUY);
    orderList.pushBack(order2);

    REQUIRE(*(orderList.begin()) == order);

    auto it = orderList.begin();
    ++it;
    REQUIRE(*it == order2);
    ++it;
    REQUIRE(it == orderList.end());
}
