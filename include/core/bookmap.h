#pragma once 

#include <atomic>    // For std::atomic
#include <cstddef>   // For size_t
#include <memory>    // For std::shared_ptr
#include <stdexcept> // For std::runtime_error
#include <string>    // For std::string
#include <vector>    // For std::vector

import orderbook.orderbook; // For OrderBook, InstrumentSymbolView, ObjectCount
import orderbook.constants; // For kMaxInstruments

/** Book is a lock-free map of instrument -> OrderBook */
namespace orderbook {

template <typename TListener>
class BookMap {
    std::atomic<std::shared_ptr<OrderBook<TListener>>> m_orderBooks[kMaxInstruments];
public:
    BookMap() {
        for(ObjectCount i = 0; i < kMaxInstruments; i++) {
            m_orderBooks[i].store(nullptr);
        }
    }
    
    std::shared_ptr<OrderBook<TListener>> getOrCreate(InstrumentSymbolView instrument, TListener& listener) { // Renamed to camelCase
        auto hash = std::hash<std::string_view>{}(instrument);
        const auto start = hash % kMaxInstruments;
        auto orderBook = m_orderBooks[start].load();
        if (orderBook != nullptr && orderBook->m_instrument == instrument) return orderBook;
        
        auto new_book = std::make_shared<OrderBook<TListener>>(std::string(instrument), listener); // Renamed to new_snake_case
        auto index = start;
        while (true) {
            if (orderBook != nullptr) {
                index = (index + 1) % kMaxInstruments;
                if (index == start) throw std::runtime_error("no room in books map");
                orderBook = m_orderBooks[index].load();
                if (orderBook != nullptr && orderBook->m_instrument == instrument) return orderBook;
            } else {
                if (m_orderBooks[index].compare_exchange_weak(orderBook, new_book)) {
                    return new_book;
                }
            }
        }
    }
    
    std::shared_ptr<OrderBook<TListener>> getOrderBook(InstrumentSymbolView instrument) const { // Renamed to camelCase
        auto hash = std::hash<std::string_view>{}(instrument);
        const auto start = hash % kMaxInstruments;
        auto orderBook = m_orderBooks[start].load();
        if (orderBook != nullptr && orderBook->m_instrument == instrument) return orderBook;
        
        auto index = start;
        while (true) {
            if (orderBook != nullptr) {
                index = (index + 1) % kMaxInstruments;
                if (index == start) return nullptr;
                orderBook = m_orderBooks[index].load();
                if (orderBook != nullptr && orderBook->m_instrument == instrument) return orderBook;
            } else {
                index = (index + 1) % kMaxInstruments;
                if (index == start) return nullptr;
                orderBook = m_orderBooks[index].load();
            }
        }
    }
    
    std::vector<std::string> instruments() const {
        std::vector<std::string> result;
        for (ObjectCount i = 0; i < kMaxInstruments; i++) {
            auto orderBook = m_orderBooks[i].load();
            if (orderBook != nullptr) {
                result.push_back(orderBook->m_instrument);
            }
        }
        return result;
    }
};

} // namespace orderbook