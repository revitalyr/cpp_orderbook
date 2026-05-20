#include <catch2/catch_test_macros.hpp>

import orderbook;
import orderbook.semantic_types; // For ExchangeId, Price, Quantity

using namespace orderbook;
TEST_CASE("Order basic creation", "[order]") { // Renamed to camelCase
    auto order1 = TestOrder::create("myorder", ExchangeId(1), Price(100), Quantity(10), Order::Side::BUY, ExchangeId(1)); // Renamed to camelCase
    REQUIRE(order1->orderId() == "myorder"); // Renamed to camelCase
    auto order2 = TestOrder::create("myorder2", ExchangeId(1), Price(100), Quantity(10), Order::Side::BUY, ExchangeId(2)); // Renamed to camelCase
    REQUIRE(order2->orderId() == "myorder2"); // Renamed to camelCase
}
