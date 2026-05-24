#include <catch2/catch_test_macros.hpp>

#include <boost/intrusive/list.hpp>
import orderbook;

using namespace orderbook;
static orderbook::ExchangeListener g_dummyListener;

TEST_CASE("BookMap basic operations", "[bookmap]") {
    BookMap<ExchangeListener> bookMap;

    auto orderBook = bookMap.getOrderBook("dummy");
    REQUIRE(orderBook == nullptr);

    orderBook = bookMap.getOrCreate("dummy", g_dummyListener);
    REQUIRE(orderBook != nullptr);

    auto orderBook2 = bookMap.getOrCreate("dummy", g_dummyListener);
    REQUIRE(orderBook == orderBook2);

    auto orderBook3 = bookMap.getOrderBook("dummy");
    REQUIRE(orderBook == orderBook2);
    REQUIRE(orderBook2 == orderBook3);
}

TEST_CASE("BookMap instruments", "[bookmap]") {
    BookMap<ExchangeListener> bookMap;
    
    auto orderBook = bookMap.getOrCreate("dummy", g_dummyListener);
    REQUIRE(orderBook != nullptr);
    
    // Check if orderBook has instruments
    if (orderBook && orderBook->instruments().size() > 0) {
        REQUIRE(orderBook->instruments()[0] == "dummy");
        REQUIRE(orderBook->instruments().size() == 1);
    } else {
        REQUIRE(orderBook->instruments().size() == 0);
    }
    
    // Check if orderBook has exactly one instrument
    if (orderBook && orderBook->instruments().size() == 1) {
        REQUIRE(orderBook->instruments()[0] == "dummy");
    } else {
        REQUIRE(orderBook->instruments().size() == 1);
    }
}
