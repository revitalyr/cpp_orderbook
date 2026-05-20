#include <catch2/catch_test_macros.hpp>

#include "core/bookmap.h"
#include "core/orderbook.h"
#include "core/exchange.h"

using namespace orderbook;
static orderbook::ExchangeListener g_dummyListener; // Renamed to g_camelCase

TEST_CASE("BookMap basic operations", "[bookmap]") {
    BookMap<ExchangeListener> bookMap; // Renamed to camelCase

    auto orderBook = bookMap.getOrderBook("dummy");
    REQUIRE(orderBook == nullptr);

    orderBook = bookMap.getOrCreate("dummy", g_dummyListener);
    REQUIRE(orderBook != nullptr);

    auto orderBook2 = bookMap.getOrCreate("dummy", g_dummyListener); // Renamed to camelCase
    REQUIRE(orderBook == orderBook2);

    auto orderBook3 = bookMap.getOrderBook("dummy");
    REQUIRE(orderBook == orderBook2);
    REQUIRE(orderBook2 == orderBook3);
}

TEST_CASE("BookMap instruments", "[bookmap]") { // Renamed to camelCase
    BookMap<ExchangeListener> bookMap; // Renamed to camelCase
    
    auto orderBook = bookMap.getOrCreate("dummy", g_dummyListener); // Renamed to camelCase, g_camelCase
    REQUIRE(orderBook != nullptr); // Renamed to camelCase
    
    // Check if orderBook has instruments // Renamed to camelCase
    if (orderBook && orderBook->instruments().size() > 0) { // Renamed to camelCase
        REQUIRE(orderBook->instruments()[0] == "dummy"); // Renamed to camelCase
        REQUIRE(orderBook->instruments().size() == 1); // Renamed to camelCase
    } else {
        REQUIRE(orderBook->instruments().size() == 0); // Renamed to camelCase
    }
    
    // Check if orderBook has exactly one instrument // Renamed to camelCase
    if (orderBook && orderBook->instruments().size() == 1) { // Renamed to camelCase
        REQUIRE(orderBook->instruments()[0] == "dummy"); // Renamed to camelCase
    } else {
        REQUIRE(orderBook->instruments().size() == 1); // Renamed to camelCase
    }
}
