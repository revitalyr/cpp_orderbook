#include <catch2/catch_test_macros.hpp>

#include <boost/intrusive/list.hpp>
import orderbook;

using namespace orderbook;

TEST_CASE("OrderList basic operations", "[orderlist]") {
    OrderList orderList(100);
    REQUIRE(orderList.begin() == orderList.end());

    auto order = TestOrder::createOrder(ExchangeId(1), Price(100), Quantity(10), Order::Side::BUY);
    orderList.pushBack(*order);

    REQUIRE(orderList.begin() != orderList.end());
    REQUIRE(&*orderList.begin() == order.get());
}

TEST_CASE("OrderList iterator", "[orderlist]") {
    OrderList orderList(100);
    REQUIRE(orderList.begin() == orderList.end());

    auto order = TestOrder::createOrder(1, 100, 10, Order::Side::BUY);
    orderList.pushBack(*order);

    REQUIRE(orderList.begin() != orderList.end());
    REQUIRE(&*orderList.begin() == order.get());

    auto order2 = TestOrder::createOrder(2, 100, 10, Order::Side::BUY);
    orderList.pushBack(*order2);
    
    REQUIRE(&*orderList.begin() == order.get());
    auto it = orderList.begin();
    ++it;
    REQUIRE(&*it == order2.get());
    ++it;
    REQUIRE(it == orderList.end());
}
