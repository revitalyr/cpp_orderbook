#include <catch2/catch_test_macros.hpp>

#include "core/test.h"

using namespace orderbook;
TEST_CASE("OrderMap basic operations", "[ordermap]") {
    OrderMap orderMap;

    auto order = TestOrder::createOrder(ExchangeId(1), Price(100), Quantity(10), Order::Side::BUY);
    REQUIRE(orderMap.get(1) == nullptr);

    orderMap.add(order);
    REQUIRE(orderMap.get(1) == order);
    
    // Test that we can retrieve the order we just added
    auto retrievedOrder = orderMap.get(1);
    REQUIRE(retrievedOrder == order);
    REQUIRE(retrievedOrder != nullptr);
    REQUIRE(retrievedOrder->m_exchangeId == 1);
}
