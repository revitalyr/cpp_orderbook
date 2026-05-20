#include <catch2/catch_test_macros.hpp>

#include "core/test.h"

using namespace orderbook;
TEST_CASE("Order basic creation", "[order]") { // Renamed to camelCase
    auto order1 = TestOrder::create(EngineConstants::kTestSessionId, "myorder", Price(100), Quantity(10), Order::Side::BUY, ExchangeId(1)); // Renamed to camelCase
    REQUIRE(order1->orderId() == "myorder"); // Renamed to camelCase
    auto order2 = TestOrder::create(EngineConstants::kTestSessionId, "myorder2", Price(100), Quantity(10), Order::Side::BUY, ExchangeId(2)); // Renamed to camelCase
    REQUIRE(order2->orderId() == "myorder2"); // Renamed to camelCase
}
