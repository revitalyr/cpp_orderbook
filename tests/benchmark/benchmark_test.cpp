#include <algorithm>
#include <chrono> // For std::chrono
#include <iostream> // For std::cout, std::cerr
#include <memory> // For std::shared_ptr
#include <random>
#include <stdexcept>
#include <string> // For std::string
#include <thread> // For std::thread::hardware_concurrency

#include "core/exchange.h"
#include "core/orderbook.h"
#include "core/order.h"

struct TestListener : ExchangeListener {
    FailureCount m_tradeCount = 0;
    // Use thread_local for multithreaded benchmarks if needed, but this is single-threaded
    // static inline thread_local FailureCount tl_tradeCount = 0;
    void onTrade(const Trade& /*trade*/) override {
        m_tradeCount++;
    }
};

void insertOrders(const bool withTrades, const int priceLevels) {

    TestListener listener;
    OrderBook<TestListener> ob(orderbook::kDummyInstrument,listener);

    static const ObjectCount kNumOrders = 5000000; // Renamed to kPascalCase
    static const ObjectCount kTotalOrders = kNumOrders * 2;

    orderbook::OrderPool::reserve(kTotalOrders);

    auto start = std::chrono::system_clock::now();

    for(ObjectCount i=0; i<kNumOrders; i++) {
        auto order = TestOrder::create(orderbook::ExchangeId(i), orderbook::Price(5000.0 + 1 * (i % priceLevels)), orderbook::Quantity(10), orderbook::Order::Side::BUY); // Renamed to camelCase
        ob.insertOrder(order);
    }
    for(ObjectCount i=0; i<kNumOrders; i++) {
        auto order = TestOrder::create(orderbook::ExchangeId(kNumOrders + i), orderbook::Price((withTrades ? 5000.0 : 10000.0) + 1 * (i % priceLevels)), orderbook::Quantity(10), orderbook::Order::Side::SELL); // Renamed to camelCase
        ob.insertOrder(order);
    }
    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
    std::cout << "insert orders " << priceLevels << " levels, usec per order " << (duration.count()/(double)(kTotalOrders)) << ", orders per sec " << (int)(((kTotalOrders)/(duration.count()/1000000.0))) << "\n";
    std::cout << "insert orders " << priceLevels << " levels with trade match % " << (static_cast<double>(listener.m_tradeCount) * 100.0 / kTotalOrders) << "\n";
}

/** tests the time to remove an order at a random position in the OrderBook */
void cancelOrders(const int priceLevels) {
    TestListener listener; // Use TestListener for consistency
    OrderBook<TestListener> ob(orderbook::kDummyInstrument,listener);

    static const ObjectCount kNumOrders = 1000000; // Renamed to kPascalCase

    orderbook::OrderPool::reserve(kNumOrders);

    std::vector<std::string> output;

    std::vector<std::shared_ptr<Order>> orders;
    orders.reserve(kNumOrders); // Renamed to kPascalCase

    for(ObjectCount i=0; i<kNumOrders; i++) { // Renamed to kPascalCase
        auto order = TestOrder::create(orderbook::ExchangeId(i), orderbook::Price(100.0 + 1 * (i % priceLevels)), orderbook::Quantity(10), orderbook::Order::Side::BUY); // Renamed to camelCase
        ob.insertOrder(order);
        orders.push_back(order);
    }

    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(std::begin(orders),std::end(orders),g);

    auto start = std::chrono::system_clock::now();
    for(ObjectCount i=0; i<kNumOrders; i++) { // Renamed to kPascalCase
        ob.cancelOrder(orders[i]);
    }
    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start);

    std::cout << "cancel orders "<<priceLevels<<" levels, usec per order " << (duration.count()/(double)(kNumOrders)) << ", orders per sec " << (int)(((kNumOrders)/(duration.count()/1000000.0))) << "\n"; // Renamed to kPascalCase
}

int main() {
    std::cout << "sizeof Fixed " << sizeof(orderbook::F) << " number of cores " << std::thread::hardware_concurrency() << "\n";
    try {
        insertOrders(false,1000);
        insertOrders(true,1000);
        cancelOrders(1000);
        insertOrders(false,10);
        insertOrders(true,10);
        cancelOrders(10);
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error occurred: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred." << std::endl;
        return 1;
    }
    return 0;
}