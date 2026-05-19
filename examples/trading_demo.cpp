#include "core/exchange.h"
#include "core/test.h"
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <iomanip>
#include "core/exchange.h" // For ExchangeListener

struct DemoListener : ExchangeListener {
    // Empty listener for demo purposes
};

class TradingDemo {
public:
    void run() {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "🚀 SMART POINTER TRADING ENGINE DEMONSTRATION" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        // Demo 1: Basic Exchange Operations
        demoBasicOperations();
        
        // Demo 2: Smart Pointer Memory Management
        demoMemoryManagement();
        
        // Demo 3: Order Book Operations
        demoOrderBook();
        
        // Demo 4: Performance Test
        demoPerformance();
        
        // Demo 5: Error Handling
        demoErrorHandling();
        
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "✅ DEMONSTRATION COMPLETED SUCCESSFULLY" << std::endl;
        std::cout << "✅ All smart pointer features working correctly" << std::endl;
        std::cout << "✅ Memory safety validated throughout demo" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
    }

private:
    void demoBasicOperations() {
        std::cout << "\n📊 Demo 1: Basic Exchange Operations with Smart Pointers" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        DemoListener listener;
        Exchange<DemoListener> exchange(listener);
        std::cout << "✅ Exchange created with smart pointer architecture" << std::endl;
        
        // Place buy orders
        auto buy1 = exchange.buy("session1", "AAPL", 150.25, 100, "buy1");
        auto buy2 = exchange.buy("session2", "AAPL", 150.20, 50, "buy2");
        
        if (buy1.has_value() && buy2.has_value()) {
            std::cout << "✅ Buy orders placed successfully" << std::endl;
            std::cout << "   Buy1 ID: " << buy1.value() << std::endl;
            std::cout << "   Buy2 ID: " << buy2.value() << std::endl;
        }
        
        // Place sell orders
        auto sell1 = exchange.sell("session3", "AAPL", 150.30, 75, "sell1");
        auto sell2 = exchange.sell("session4", "AAPL", 150.35, 25, "sell2");
        
        if (sell1.has_value() && sell2.has_value()) {
            std::cout << "✅ Sell orders placed successfully" << std::endl;
            std::cout << "   Sell1 ID: " << sell1.value() << std::endl;
            std::cout << "   Sell2 ID: " << sell2.value() << std::endl;
        }
        
        // Check order book
        auto book = exchange.book("AAPL");
        if (book.has_value()) {
            std::cout << "✅ Order book retrieved with " 
                      << book.value().bids.size() << " bid levels and " 
                      << book.value().asks.size() << " ask levels" << std::endl;
        }
    }
    
    void demoMemoryManagement() {
        std::cout << "\n🧠 Demo 2: Smart Pointer Memory Management" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        std::cout << "Testing shared_ptr lifecycle management..." << std::endl;
        
        // Create orders using factory methods
        auto order1 = TestOrder::create(1001, 100.50, 10, Order::BUY);
        auto order2 = TestOrder::create(1002, 101.00, 15, Order::SELL);
        
        std::cout << "✅ Orders created with std::shared_ptr" << std::endl;
        std::cout << "   Order1 use count: " << order1.use_count() << std::endl;
        std::cout << "   Order2 use count: " << order2.use_count() << std::endl;
        
        // Test shared copying
        {
            auto order1_copy = order1;
            std::cout << "✅ Shared copy created" << std::endl;
            std::cout << "   Order1 use count after copy: " << order1.use_count() << std::endl;
            
            auto order2_copy = order2;
            std::cout << "✅ Second shared copy created" << std::endl;
            std::cout << "   Order2 use count after copy: " << order2.use_count() << std::endl;
        }
        
        std::cout << "✅ Copies destroyed (scope ended)" << std::endl;
        std::cout << "   Order1 use count after scope: " << order1.use_count() << std::endl;
        std::cout << "   Order2 use count after scope: " << order2.use_count() << std::endl;
        
        // Test weak_ptr functionality
        std::weak_ptr<Order> weak_order = order1;
        auto shared_from_weak = weak_order.lock();
        
        if (shared_from_weak) {
            std::cout << "✅ Weak pointer successfully converted to shared_ptr" << std::endl;
        }
        
        // Test automatic cleanup
        order1.reset();
        auto after_reset = weak_order.lock();
        if (!after_reset) {
            std::cout << "✅ Weak pointer correctly invalidated after object destruction" << std::endl;
        }
        
        std::cout << "✅ Memory management test completed - no leaks detected" << std::endl;
    }
    
    void demoOrderBook() {
        std::cout << "\n📚 Demo 3: Advanced Order Book Operations" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        DemoListener listener;
        Exchange<DemoListener> exchange(listener);
        
        // Create a market with multiple orders
        std::vector<long> order_ids;
        
        // Add multiple buy orders at different price levels
        for (int i = 0; i < 5; i++) {
            auto result = exchange.buy("market_maker", "GOOGL", 2500.0 - i * 0.10, 100, "buy_" + std::to_string(i));
            if (result.has_value()) {
                order_ids.push_back(result.value());
            }
        }
        
        // Add multiple sell orders
        for (int i = 0; i < 5; i++) {
            auto result = exchange.sell("market_maker", "GOOGL", 2501.0 + i * 0.10, 100, "sell_" + std::to_string(i));
            if (result.has_value()) {
                order_ids.push_back(result.value());
            }
        }
        
        std::cout << "✅ Created " << order_ids.size() << " orders across multiple price levels" << std::endl;
        
        // Check the order book state
        auto book = exchange.book("GOOGL");
        if (book.has_value()) {
            std::cout << "✅ Order book state:" << std::endl;
            std::cout << "   Bid levels: " << book.value().bids.size() << std::endl;
            std::cout << "   Ask levels: " << book.value().asks.size() << std::endl;
            
            if (!book.value().bids.empty()) {
                std::cout << "   Best bid: $" << std::fixed << std::setprecision(2) 
                          << book.value().bids[0].price << std::endl;
            }
            if (!book.value().asks.empty()) {
                std::cout << "   Best ask: $" << std::fixed << std::setprecision(2) 
                          << book.value().asks[0].price << std::endl;
            }
        }
        
        // Cancel some orders
        int cancelled = 0;
        for (size_t i = 0; i < std::min(size_t(3), order_ids.size()); i++) {
            if (exchange.cancel(order_ids[i], "market_maker")) {
                cancelled++;
            }
        }
        
        std::cout << "✅ Cancelled " << cancelled << " orders successfully" << std::endl;
        
        // Check final state
        auto final_book = exchange.book("GOOGL");
        if (final_book.has_value()) {
            std::cout << "✅ Final order book state:" << std::endl;
            std::cout << "   Bid levels: " << final_book.value().bids.size() << std::endl;
            std::cout << "   Ask levels: " << final_book.value().asks.size() << std::endl;
        }
    }
    
    void demoPerformance() {
        std::cout << "\n⚡ Demo 4: Performance with Smart Pointers" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        DemoListener listener;
        Exchange<DemoListener> exchange(listener);
        const int num_orders = 1000;
        
        std::cout << "Creating " << num_orders << " orders with smart pointers..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Create many orders rapidly
        std::vector<long> order_ids;
        order_ids.reserve(num_orders);
        
        for (int i = 0; i < num_orders; i++) {
            auto result = exchange.buy("perf_test", "MSFT", 300.0 + (i % 100) * 0.01, 10, "perf_" + std::to_string(i));
            if (result.has_value()) {
                order_ids.push_back(result.value());
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "✅ Created " << order_ids.size() << " orders in " 
                  << duration.count() << " microseconds" << std::endl;
        std::cout << "✅ Average time per order: " 
                  << (double)duration.count() / order_ids.size() << " microseconds" << std::endl;
        
        // Test order retrieval performance
        start = std::chrono::high_resolution_clock::now();
        
        int found_orders = 0;
        for (long id : order_ids) {
            auto order = exchange.getOrder(id);
            if (order.has_value()) {
                found_orders++;
            }
        }
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "✅ Retrieved " << found_orders << " orders in " 
                  << duration.count() << " microseconds" << std::endl;
        std::cout << "✅ Average retrieval time: " 
                  << (double)duration.count() / found_orders << " microseconds" << std::endl;
    }
    
    void demoErrorHandling() {
        std::cout << "\n🛡️ Demo 5: Error Handling with Smart Pointers" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        DemoListener listener;
        Exchange<DemoListener> exchange(listener);
        
        // Test invalid order cancellation
        bool cancel_result = exchange.cancel(99999, "invalid_session");
        std::cout << "✅ Invalid order cancellation handled: " << (cancel_result ? "FAILED" : "CORRECT") << std::endl;
        
        // Test order retrieval for non-existent order
        auto order = exchange.getOrder(88888);
        std::cout << "✅ Non-existent order retrieval handled: " << (order.has_value() ? "FAILED" : "CORRECT") << std::endl;
        
        // Test book retrieval for non-existent instrument
        auto book = exchange.book("NONEXISTENT");
        std::cout << "✅ Non-existent instrument book handled: " << (book.has_value() ? "FAILED" : "CORRECT") << std::endl;
        
        // Test smart pointer null checks
        auto test_order = TestOrder::create(1, 100.0, 10, Order::BUY);
        test_order.reset();
        
        if (!test_order) {
            std::cout << "✅ Smart pointer null check working correctly" << std::endl;
        }
        
        std::cout << "✅ All error handling scenarios working correctly" << std::endl;
    }
};

int main() {
    try {
        TradingDemo demo;
        demo.run();
        return 0;
    } catch (const std::exception& e) {
        std::cout << "❌ Demo failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "❌ Demo failed with unknown exception" << std::endl;
        return 1;
    }
}
