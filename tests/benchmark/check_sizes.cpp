#include <iostream>
#include "core/order.h"
using namespace orderbook;
int main() {
    std::cout << "sizeof(Order) = " << sizeof(Order) << "\n";
    std::cout << "sizeof(MemoryPool<Order>) = " << sizeof(MemoryPool<Order>) << "\n";
    std::cout << "sizeof(PriceLevels) = " << sizeof(PriceLevels) << "\n";
    std::cout << "sizeof(OrderBook<TestListener>) = " << sizeof(OrderBook<int>) << "\n";
    std::cout << "sizeof(PoolNode) = " << sizeof(MemoryPool<Order>::PoolNode) << "\n";
    // Check reserved memory
    for (int n : {1000, 10000, 100000, 1000000}) {
        OrderPool::reserve(n);
        std::cout << "Reserved " << n << ": capacity=" << OrderPool::instance().capacity() << " allocated=" << OrderPool::instance().allocated_count() << "\n";
    }
    return 0;
}
