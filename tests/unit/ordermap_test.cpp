#include <catch2/catch_test_macros.hpp>

#include "core/test.h"

using namespace orderbook;
TEST_CASE("OrderMap basic operations", "[ordermap]") {
    OrderMap orderMap; // Renamed to camelCase

    auto order = TestOrder::create(ExchangeId(1), Price(100), Quantity(10), Order::Side::BUY); // Renamed to camelCase
    REQUIRE(orderMap.get(1) == nullptr); // Renamed to camelCase

    orderMap.add(order); // Renamed to camelCase
    REQUIRE(orderMap.get(1) == order); // Renamed to camelCase
    
    // Test that we can retrieve the order we just added
    auto retrievedOrder = orderMap.get(1); // Renamed to camelCase
    REQUIRE(retrievedOrder == order); // Renamed to camelCase
    REQUIRE(retrievedOrder != nullptr); // Renamed to camelCase
    REQUIRE(retrievedOrder->m_exchangeId == 1); // Renamed to m_snake_case
}
