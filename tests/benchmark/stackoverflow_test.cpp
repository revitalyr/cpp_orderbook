#include <iostream>
#include "core/order.h"

using namespace orderbook;

int main() {
    std::cout << "sizeof(Order) = " << sizeof(Order) << "\n";
    std::cout << "sizeof(MemoryPool<Order>::PoolNode) = " << sizeof(MemoryPool<Order>) << "\n";
    std::cout << "Reserving 10000...\n";
    OrderPool::reserve(10000);
    std::cout << "Reserving 100000...\n";
    OrderPool::reserve(100000);
    std::cout << "Reserving 1000000...\n";
    OrderPool::reserve(1000000);
    std::cout << "Reserving 10000000...\n";
    OrderPool::reserve(10000000);
    std::cout << "Done.\n";
    return 0;
}
