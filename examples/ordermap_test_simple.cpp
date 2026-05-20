#include <iostream>
#include <cassert>

import orderbook;

using namespace orderbook;

int main() {
    OrderMap map; // Renamed to camelCase
    
    // Test single order to avoid any recursion, using smart pointers
    auto o = TestOrder::create(ExchangeId(1), Price(100), Quantity(10), Order::Side::BUY); // Renamed to camelCase
    
    // Verify initial state
    assert(map.get(1)==nullptr);
    
    // Add order
    map.add(o);
    
    // Verify order was added
    assert(map.get(1)==o);
    assert(map.get(1)->m_exchangeId == 1);
    
    // Test that get returns null for non-existent order
    assert(map.get(999)==nullptr);

    std::cout << "OrderMap simple test passed!" << std::endl;
    return 0;
}
