#include <algorithm>
#include <chrono>
#include <sstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
#include <array>

#include "core/exchange.h"
#include "core/orderbook.h"
#include "core/test.h"

struct TestListener : OrderBookListener {
    int m_tradeCount = 0;
    void onTrade(const Trade& /*trade*/) override {
        m_tradeCount++;
    }
};

void insertOrders(const bool withTrades,const int priceLevels) {

    TestListener listener;
    OrderBook<TestListener> ob(kDummyInstrument,listener);

    static const int kNumOrders = 5000000;
    static const int kTotalOrders = kNumOrders * 2;

    orderbook::OrderPool::reserve(kTotalOrders);

    auto start = std::chrono::system_clock::now();

    for(int i=0;i<kNumOrders;i++) {
        auto order = TestOrder::create(i,5000.0 + 1 * (i%priceLevels),10,Order::Side::BUY);
        ob.insertOrder(order);
    }
    for(int i=0;i<kNumOrders;i++) {
        auto order = TestOrder::create(kNumOrders+i,(withTrades ? 5000.0 : 10000.0) + 1 * (i%priceLevels),10,Order::Side::SELL);
        ob.insertOrder(order);
    }
    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
    std::cout << "insert orders " << priceLevels << " levels, usec per order " << (duration.count()/(double)(kNumOrders*2)) << ", orders per sec " << (int)(((kNumOrders*2)/(duration.count()/1000000.0))) << "\n";
    std::cout << "insert orders " << priceLevels << " levels with trade match % " << (listener.m_tradeCount*100/kTotalOrders) << "\n";
}

/** tests the time to remove an order at a random position in the OrderBook */
void cancelOrders(const int priceLevels) {
    TestListener listener; // Use TestListener for consistency
    OrderBook<TestListener> ob(kDummyInstrument,listener);

    static const int kNumOrders = 1000000;

    orderbook::OrderPool::reserve(kNumOrders);

    std::vector<std::string> output;

    std::vector<std::shared_ptr<Order>> orders;
    orders.reserve(kNumOrders); // Renamed to kPascalCase

    for(int i=0;i<kNumOrders;i++) { // Renamed to kPascalCase
        auto order = TestOrder::create(i,100.0 + 1 * (i%priceLevels),10,Order::Side::BUY); // Renamed to camelCase
        ob.insertOrder(order); // Renamed to camelCase
        orders.push_back(order);
    }

    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(std::begin(orders),std::end(orders),g);

    auto start = std::chrono::system_clock::now();
    for(int i=0;i<kNumOrders;i++) { // Renamed to kPascalCase
        ob.cancelOrder(orders[i]);
    }
    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end-start);

    std::cout << "cancel orders "<<priceLevels<<" levels, usec per order " << (duration.count()/(double)(kNumOrders)) << ", orders per sec " << (int)(((kNumOrders)/(duration.count()/1000000.0))) << "\n"; // Renamed to kPascalCase
}

int main() {
    std::cout << "sizeof Fixed " << sizeof(F) << " number of cores " << std::thread::hardware_concurrency() << "\n";
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