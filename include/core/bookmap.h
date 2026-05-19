#pragma once 

#include <stdexcept>
#include <cstddef>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "orderbook.h"
#include "constants.h"

/** Book is a lock-free map of instrument -> OrderBook */
template <typename TListener>
class BookMap {
    std::atomic<std::shared_ptr<OrderBook<TListener>>> m_orderBooks[kMaxInstruments]; // Array of atomic shared pointers to OrderBook
public:
    BookMap() {
        for(ObjectCount i = 0; i < kMaxInstruments; i++) {
            m_orderBooks[i].store(nullptr);
        }
    }
    
    std::shared_ptr<OrderBook<TListener>> getOrCreate(InstrumentSymbolView instrument, TListener& listener) {
        auto hash = std::hash<std::string_view>{}(instrument);
        const auto start = hash % kMaxInstruments;
        auto orderBook = m_orderBooks[start].load();
        if (orderBook != nullptr && orderBook->m_instrument == instrument) return orderBook;
        
        auto new_book = std::make_shared<OrderBook<TListener>>(std::string(instrument), listener);
        auto index = start;
        while (true) {
            if (orderBook != nullptr) {
                index = (index + 1) % kMaxInstruments;
                if (index == start) throw std::runtime_error("no room in books map");
                orderBook = m_orderBooks[index].load();
                if (orderBook != nullptr && orderBook->m_instrument == instrument) return orderBook;
            } else {
                if (m_orderBooks[index].compare_exchange_weak(orderBook, new_book)) { // Renamed to camelCase
                    return new_book;
                }
            }
        }
    }
    
    std::shared_ptr<OrderBook<TListener>> getOrderBook(InstrumentSymbolView instrument) const {
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