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
#include "core/test.h"
#include "safety/production_safety.h"

using namespace orderbook;
struct TestListener : ExchangeListener {
    FailureCount m_tradeCount = 0;
    // Use thread_local for multithreaded benchmarks if needed, but this is single-threaded
    // static inline thread_local FailureCount tl_tradeCount = 0;
    void onTrade(const Trade& /*trade*/) override {
        m_tradeCount++;
    }
};

void insertOrders(const bool withTrades, const int priceLevels) {

    std::cout << "  [insertOrders] creating OrderBook...\n";
    TestListener listener;
    OrderBook<TestListener> ob(orderbook::kDefaultInstrument,listener);

    static const ObjectCount kNumOrders = 2000000; // Renamed to kPascalCase (was 5M — stack overflow in Debug)
    static const ObjectCount kTotalOrders = kNumOrders * 2;

    std::cout << "  [insertOrders] reserving " << kTotalOrders << " pool nodes...\n";
    orderbook::OrderPool::reserve(kTotalOrders);
    std::cout << "  [insertOrders] reserve done.\n";

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
    std::cout << "  [cancelOrders] creating OrderBook...\n";
    TestListener listener; // Use TestListener for consistency
    OrderBook<TestListener> ob(orderbook::kDefaultInstrument,listener);

    static const ObjectCount kNumOrders = 1000000; // Renamed to kPascalCase

    std::cout << "  [cancelOrders] reserving " << kNumOrders << " pool nodes...\n";
    orderbook::OrderPool::reserve(kNumOrders);
    std::cout << "  [cancelOrders] reserve done.\n";

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
    ::ProductionSafety::enableSafety(false);
    
    FILE* log = fopen("benchmark_debug.log", "w");
    if (log) { fprintf(log, "DEBUG: main() entered\n"); fflush(log); }
    std::cout << "sizeof Fixed " << sizeof(orderbook::F) << " number of cores " << std::thread::hardware_concurrency() << "\n";
    if (log) { fprintf(log, "DEBUG: after sizeof\n"); fflush(log); }
    if (log) { fprintf(log, "DEBUG: calling insertOrders(false,1000)...\n"); fflush(log); }
    std::cout << "Calling insertOrders(false, 1000)...\n";
    try {
        insertOrders(false,1000);
        if (log) { fprintf(log, "DEBUG: insertOrders(false,1000) done\n"); fflush(log); }
        insertOrders(true,1000);
        if (log) { fprintf(log, "DEBUG: insertOrders(true,1000) done\n"); fflush(log); }
        cancelOrders(1000);
        if (log) { fprintf(log, "DEBUG: cancelOrders(1000) done\n"); fflush(log); }
        insertOrders(false,10);
        if (log) { fprintf(log, "DEBUG: insertOrders(false,10) done\n"); fflush(log); }
        insertOrders(true,10);
        if (log) { fprintf(log, "DEBUG: insertOrders(true,10) done\n"); fflush(log); }
        cancelOrders(10);
        if (log) { fprintf(log, "DEBUG: cancelOrders(10) done\n"); fflush(log); }
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