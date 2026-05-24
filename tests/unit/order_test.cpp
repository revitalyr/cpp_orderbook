#include <catch2/catch_test_macros.hpp>

#include <boost/intrusive/list.hpp>
import orderbook;

using namespace orderbook;
TEST_CASE("Order basic creation", "[order]") {
    auto order1 = TestOrder::create(EngineConstants::kTestSessionId, "myorder", Price(100), Quantity(10), Order::Side::BUY, ExchangeId(1));
    REQUIRE(order1->orderId() == "myorder");
    auto order2 = TestOrder::create(EngineConstants::kTestSessionId, "myorder2", Price(100), Quantity(10), Order::Side::BUY, ExchangeId(2));
    REQUIRE(order2->orderId() == "myorder2");
}
