#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>

#include <boost/intrusive/list.hpp>
import orderbook;

using namespace orderbook;

struct TestListener : ExchangeListener {
    FailureCount m_tradeCount = 0;
    // Use thread_local for multithreaded benchmarks if needed, but this is single-threaded
    // static inline thread_local FailureCount tl_tradeCount = 0;
    void onTrade(const Trade& /*trade*/) override {
        m_tradeCount++;
    }
};

static std::atomic<int64_t> g_insertCount{0};

void insertOrders(const bool withTrades, const int priceLevels) {

    std::cout << "  [insertOrders] creating OrderBook...\n" << std::flush;
    TestListener listener;
    OrderBook<TestListener> ob(orderbook::kDefaultInstrument,listener);
    std::cout << "  [insertOrders] OrderBook created\n" << std::flush;

    static const ObjectCount kNumOrders = 2000000; // (was 5M — stack overflow in Debug)
    static const ObjectCount kTotalOrders = kNumOrders * 2;

    std::cout << "  [insertOrders] reserving " << kTotalOrders << " pool nodes...\n" << std::flush;
    orderbook::OrderPool::reserve(kTotalOrders);
    std::cout << "  [insertOrders] reserve done.\n" << std::flush;

    std::cout << "  [insertOrders] starting first loop (BUY orders)...\n" << std::flush;

    auto start = std::chrono::system_clock::now();

    for(ObjectCount i=0; i<kNumOrders; i++) {
        auto order = TestOrder::createOrder(orderbook::ExchangeId(i), orderbook::Price(5000.0 + 1 * (i % priceLevels)), orderbook::Quantity(10), orderbook::Order::Side::BUY);
        if (!order) { printf("### NULL order at i=%zu\n", (size_t)i); fflush(stdout); std::abort(); }
        ob.insertOrder(order);
    }
    for(ObjectCount i=0; i<kNumOrders; i++) {
        auto order = TestOrder::createOrder(orderbook::ExchangeId(kNumOrders + i), orderbook::Price((withTrades ? 5000.0 : 10000.0) + 1 * (i % priceLevels)), orderbook::Quantity(10), orderbook::Order::Side::SELL);
        if (!order) { printf("### NULL order at i=%zu (SELL)\n", (size_t)i); fflush(stdout); std::abort(); }
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

    static const ObjectCount kNumOrders = 1000000;

    std::cout << "  [cancelOrders] reserving " << kNumOrders << " pool nodes...\n";
    orderbook::OrderPool::reserve(kNumOrders);
    std::cout << "  [cancelOrders] reserve done.\n";

    std::vector<std::string> output;

    std::vector<std::shared_ptr<Order>> orders;
    orders.reserve(kNumOrders);

    for(ObjectCount i=0; i<kNumOrders; i++) {
        auto order = TestOrder::createOrder(orderbook::ExchangeId(i), orderbook::Price(100.0 + 1 * (i % priceLevels)), orderbook::Quantity(10), orderbook::Order::Side::BUY);
        ob.insertOrder(order);
        orders.push_back(order);
    }

    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(std::begin(orders),std::end(orders),g);

    auto start = std::chrono::system_clock::now();
    for(ObjectCount i=0; i<kNumOrders; i++) {
        ob.cancelOrder(orders[i]);
    }
    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start);

    std::cout << "cancel orders "<<priceLevels<<" levels, usec per order " << (duration.count()/(double)(kNumOrders)) << ", orders per sec " << (int)(((kNumOrders)/(duration.count()/1000000.0))) << "\n";
}

int main() {
    printf("BENCHMARK STARTING\n");
    fflush(stdout);
    ProductionSafety::enableSafety(false);
    
    printf("sizeof Fixed %zu cores %zu\n", sizeof(orderbook::Price), (size_t)std::thread::hardware_concurrency());
    fflush(stdout);
    std::cout << "sizeof Fixed " << sizeof(orderbook::Price) << " number of cores " << std::thread::hardware_concurrency() << "\n";
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
    printf("=== ALL DONE ===\n");
    fflush(stdout);
    return 0;
}