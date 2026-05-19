#include <catch2/catch_test_macros.hpp>

#include "core/order.h"
#include "core/test.h"

TEST_CASE("Order basic creation", "[order]") {
    auto order1 = TestOrder::create("myorder", 1, 100, 10, Order::Side::BUY, 1);
    REQUIRE(order1->orderId() == "myorder");
    auto order2 = TestOrder::create("myorder2", 1, 100, 10, Order::Side::BUY, 2);
    REQUIRE(order2->orderId() == "myorder2");
}
