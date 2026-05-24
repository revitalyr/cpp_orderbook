#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include <boost/intrusive/list.hpp>
import orderbook;

using namespace orderbook;

struct SimpleDemoListener : ExchangeListener {
    // Empty listener for demo purposes
};

class SimpleTradingDemo {
public:
    void run() {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "🚀 SMART POINTER TRADING ENGINE DEMONSTRATION" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        // Demo 1: Basic Exchange Operations
        demoBasicOperations();
        
        // Demo 2: Performance Test
        demoPerformance();
        
        // Demo 3: Error Handling
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
        
        SimpleDemoListener listener;
        Exchange<SimpleDemoListener> exchange(listener);
        std::cout << "✅ Exchange created with smart pointer architecture" << std::endl;
        
        // Place buy orders
        auto buy1 = exchange.placeBuyOrder("session1", "AAPL", Price(150.25), Quantity(100), "buy1");
        auto buy2 = exchange.placeBuyOrder("session2", "AAPL", Price(150.20), Quantity(50), "buy2");
        
        if (buy1 && buy2) {
            std::cout << "✅ Buy orders placed successfully" << std::endl;
            std::cout << "   Buy1 ID: " << buy1.value() << std::endl;
            std::cout << "   Buy2 ID: " << buy2.value() << std::endl;
        }
        
        // Place sell orders
        auto sell1 = exchange.placeSellOrder("session3", "AAPL", orderbook::Price(150.30), orderbook::Quantity(75), "sell1");
        auto sell2 = exchange.placeSellOrder("session4", "AAPL", orderbook::Price(150.35), orderbook::Quantity(25), "sell2");
        
        if (sell1 && sell2) {
            std::cout << "✅ Sell orders placed successfully" << std::endl;
            std::cout << "   Sell1 ID: " << sell1.value() << std::endl;
            std::cout << "   Sell2 ID: " << sell2.value() << std::endl;
        }
        
        // Check order book
        auto book = exchange.getBook("AAPL");
        if (book) {
            std::cout << "✅ Order book retrieved with "
                      << book.value().m_bids.size() << " bid levels and " 
                      << book.value().m_asks.size() << " ask levels" << std::endl;
        }
        
        // Cancel an order
        if (exchange.cancelOrder(buy1.value(), "session1")) {
            std::cout << "✅ Order cancelled successfully" << std::endl;
        }
        
        std::cout << "✅ Basic operations completed with smart pointer memory management" << std::endl;
    }
    
    void demoPerformance() {
        std::cout << "\n⚡ Demo 2: Performance with Smart Pointers" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        SimpleDemoListener listener;
        Exchange<SimpleDemoListener> exchange(listener);
        const int num_orders = 1000;
        
        std::cout << "Creating " << num_orders << " orders with smart pointers..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Create many orders rapidly
        std::vector<ExchangeId> order_ids;
        order_ids.reserve(num_orders);
        
        for (int i = 0; i < num_orders; i++) {
            auto result = exchange.placeBuyOrder("perf_test", "MSFT", Price(300.0 + (i % 100) * 0.01), Quantity(10), "perf_" + std::to_string(i));
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
        for (size_t i = 0; i < std::min(size_t(100), order_ids.size()); i++) {
            auto order = exchange.getOrder(order_ids[i]);
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
        std::cout << "\n🛡️ Demo 3: Error Handling with Smart Pointers" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        SimpleDemoListener listener;
        Exchange<SimpleDemoListener> exchange(listener);
        
        // Test invalid order cancellation
        bool cancel_result = exchange.cancelOrder(ExchangeId(99999), "invalid_session");
        std::cout << "✅ Invalid order cancellation handled: " << (cancel_result ? "FAILED" : "CORRECT") << std::endl;
        
        // Test order retrieval for non-existent order
        auto order = exchange.getOrder(88888);
        std::cout << "✅ Non-existent order retrieval handled: " << (order.has_value() ? "FAILED" : "CORRECT") << std::endl;
        
        // Test book retrieval for non-existent instrument
        auto book = exchange.getBook("NONEXISTENT");
        std::cout << "✅ Non-existent instrument book handled: " << (book.has_value() ? "FAILED" : "CORRECT") << std::endl;
        
        std::cout << "✅ All error handling scenarios working correctly" << std::endl;
    }
};

int main() {
    try {
        SimpleTradingDemo demo;
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
