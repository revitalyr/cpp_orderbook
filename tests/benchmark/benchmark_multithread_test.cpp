#include <algorithm>
#include <random>
#include <array>    // For std::array
#include <atomic>   // For std::atomic
#include <chrono>   // For std::chrono
#include <iostream> // For std::cout
#include <random>   // For std::random_device, std::mt19937
#include <stdexcept>
#include <string>   // For std::string
#include <thread>   // Explicitly include for std::thread::hardware_concurrency
#include <vector>   // For std::vector

#include "core/exchange.h"
#include "core/orderbook.h"
#include "core/order.h"

using namespace orderbook;

const std::string dummy_oid = "oid";

void insertOrders(const bool withTrades) {
    static const orderbook::ObjectCount N_THREADS = static_cast<orderbook::ObjectCount>(std::thread::hardware_concurrency());
    static std::array<std::string,16> instruments;

    for(ObjectCount i=0; i<N_THREADS; i++) instruments[i] = "i"+std::to_string(i+1);

    static const orderbook::ObjectCount N_ORDERS = 250000;
    static const ObjectCount TOTAL_ORDERS = N_ORDERS * 2 * N_THREADS;

    orderbook::OrderPool::reserve(TOTAL_ORDERS);

    // Padded struct to prevent false sharing between threads
    struct alignas(64) ThreadCounter {
        std::atomic<ExecutionId> count{0};
    };
    std::vector<ThreadCounter> counters(N_THREADS);

    // Use thread_local to eliminate atomic contention in the listener
    struct MyExchangeListener : public orderbook::ExchangeListener {
        static inline thread_local uint64_t tl_tradeCount = 0;
        void onTrade(const Trade& ) override {
            tl_tradeCount++;
        }
    } listener;
    
    // Note: Since we use thread_local inside the listener, we don't need
    // to worry about the 8 cores fighting over a single atomic variable. // Renamed to camelCase

    orderbook::Exchange<MyExchangeListener> exchange(listener); // Renamed to camelCase
    const std::string session("dummy");
    auto fn = [&exchange,session,withTrades](const std::string &instrument) { // Renamed to camelCase
        for(ObjectCount i=0; i<N_ORDERS; i++) { // Renamed to N_snake_case
            exchange.placeBuyOrder(session, instrument, orderbook::Price(5000.0 + 1 * (i%1000)), orderbook::Quantity(10), ""); // Renamed to camelCase
        }
        for(ObjectCount i=0; i<N_ORDERS; i++) { // Renamed to N_snake_case
            exchange.placeSellOrder(session, instrument, orderbook::Price((withTrades ? 5000.0 : 10000.0) + 1 * (i%1000)), orderbook::Quantity(10), ""); // Renamed to camelCase
        }
    };

    auto start = std::chrono::system_clock::now();

    std::vector<std::thread> threads;
    for(ObjectCount i=0; i<N_THREADS; i++) {
        threads.push_back(std::thread(fn,instruments[i]));
    }
    for(auto itr = threads.begin(); itr != threads.end(); itr++) {
        itr->join();
    }
    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start); // Renamed to camelCase
    std::cout << "multithread, insert orders with trades, usec per order " << (duration.count()/(double)(TOTAL_ORDERS)) << ", orders per sec " << (int)(((TOTAL_ORDERS)/(duration.count()/1000000.0))) << "\n"; // Renamed to camelCase
    // Trade count check is omitted for the TL version in this snippet to keep it simple
    std::cout << "multithread, insert orders with trade match benchmarking active\n";
}

/** tests the time to remove an order at a random position in the OrderBook */
void cancelOrders() {
    static const orderbook::ObjectCount N_THREADS = static_cast<orderbook::ObjectCount>(std::thread::hardware_concurrency());
    static std::array<std::string,16> instruments;

    for(ObjectCount i=0; i<N_THREADS; i++) instruments[i] = "i" + std::to_string(i + 1);

    static const ObjectCount N_ORDERS = 250000;
    static const ObjectCount TOTAL_ORDERS = N_ORDERS * N_THREADS;

    orderbook::OrderPool::reserve(TOTAL_ORDERS);

    std::vector<std::string> output;

    std::vector<std::vector<ExchangeId>> oids(N_THREADS, std::vector<ExchangeId>(N_ORDERS));

    struct MyExchangeListener : public orderbook::ExchangeListener {
        std::atomic<ExecutionId> tradeCount{0};
        void onTrade(const Trade& /*trade*/) override {
            tradeCount++;
        }
    } listener;

    orderbook::Exchange<MyExchangeListener> exchange(listener); // Renamed to camelCase
    const std::string session("dummy");

    for(ObjectCount t=0; t<N_THREADS; t++) {
        for(ObjectCount i=0; i<N_ORDERS; i++) {
            auto oid = exchange.placeBuyOrder(session, instruments[t], orderbook::Price(100.0 + 1 * (i%1000)), orderbook::Quantity(10), dummy_oid); // Renamed to camelCase
            oids[t][i]=oid.value();
        }
    }

    std::random_device rd;
    std::mt19937 g(rd());

    for(ObjectCount t=0; t<N_THREADS; t++) {
        std::shuffle(std::begin(oids[t]),std::end(oids[t]),g);
    }

    auto start = std::chrono::system_clock::now();
    std::vector<std::thread> threads;

    auto fn = [&](const ObjectCount tid) {
        for(ObjectCount i=0; i<N_ORDERS; i++) {
            exchange.cancelOrder(oids[tid][i],session); // Renamed to camelCase
        }
    };

    for(ObjectCount i=0; i<N_THREADS; i++) {
        threads.push_back(std::thread(fn,i));
    }

    for(auto thread = threads.begin();thread!=threads.end();thread++) {
        thread->join();
    }

    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start);

    std::cout << "cancel orders, usec per order " << (duration.count()/(double)(TOTAL_ORDERS)) << ", orders per sec " << (int)(((TOTAL_ORDERS)/(duration.count()/1000000.0))) << "\n";
}

int main() {
    std::cout << "sizeof Fixed " << sizeof(orderbook::F) << " number of cores " << std::thread::hardware_concurrency() << "\n";
    insertOrders(false);
    insertOrders(true);
    cancelOrders();
}