#pragma once

#include <array>       // For std::array
#include <memory>      // For std::shared_ptr
#include <mutex>       // For std::lock_guard
#include <unordered_map> // For std::unordered_map
#include <unordered_set> // For std::unordered_set
#include <vector>      // For std::vector
#include "order.h"
#include "spinlock.h"
#include "constants.h"

namespace orderbook {

/**
 * @brief High-performance hash map of exchange ID -> Order
 * Uses unordered_map with shared_mutex for fast concurrent access
 */
class OrderMap {
private: // Internal state
    // Moved into orderbook namespace
    // using Order = orderbook::Order; // Not needed if Order is in orderbook namespace
    struct Shard {
        mutable orderbook::SpinLock mutex;
        std::unordered_map<ExchangeId, std::shared_ptr<Order>> map;
    };
    std::array<Shard, kOrderMapShards> m_shards;

    Shard& getShard(ExchangeId id) {
        return m_shards[static_cast<size_t>(id) % kOrderMapShards];
    }

    const Shard& getShard(ExchangeId id) const {
        return m_shards[static_cast<size_t>(id) % kOrderMapShards];
    }

public:
    OrderMap() {
        for (auto& shard : m_shards) shard.map.reserve(kDefaultOrderMapCapacity / kOrderMapShards);
    }
    
    /**
     * Add order to map
     */
    void add(std::shared_ptr<Order> order) {
        if (!order) return;

        auto& shard = getShard(order->m_exchangeId);
        std::lock_guard lock(shard.mutex);
        shard.map[order->m_exchangeId] = std::move(order);
    }
    
    /**
     * Get order by exchange ID
     */
    std::shared_ptr<Order> get(ExchangeId exchangeId) const {
        auto& shard = getShard(exchangeId);
        std::lock_guard lock(shard.mutex);
        auto it = shard.map.find(exchangeId);
        return (it != shard.map.end()) ? it->second : nullptr;
    }
    
    /**
     * Remove order by exchange ID
     */
    void remove(ExchangeId exchangeId) {
        auto& shard = getShard(exchangeId);
        std::lock_guard lock(shard.mutex);
        shard.map.erase(exchangeId);
    }
    
    /**
     * Get all orders
     */
    std::vector<std::shared_ptr<const Order>> all() const {
        std::vector<std::shared_ptr<const Order>> orders;
        
        for (const auto& shard : m_shards) {
            std::lock_guard lock(shard.mutex);
            orders.reserve(orders.size() + shard.map.size());
            for (const auto& [id, order] : shard.map) {
                orders.push_back(order);
            }
        }
        return orders;
    }
    
    /**
     * Get all unique instruments // Renamed to camelCase
     */
    std::vector<InstrumentSymbol> instruments() const {
        std::unordered_set<InstrumentSymbol> unique_instruments;
        
        for (const auto& shard : m_shards) {
            std::lock_guard lock(shard.mutex);
            for (const auto& [id, order] : shard.map) {
                unique_instruments.insert(order->instrument());
            }
        }
        
        return std::vector<InstrumentSymbol>(unique_instruments.begin(), unique_instruments.end()); // Renamed to camelCase
    }
    
    /**
     * Get order count
     */
    size_t size() const {
        size_t total = 0;
        for (const auto& shard : m_shards) {
            std::lock_guard lock(shard.mutex);
            total += shard.map.size();
        }
        return total;
    }
    
    /**
     * Clear all orders
     */
    void clear() {
        for (auto& shard : m_shards) {
            std::lock_guard lock(shard.mutex);
            shard.map.clear();
        }
    }
    
    /**
     * Reserve space for expected order count
     */
    void reserve(size_t n) {
        size_t perShard = n / kOrderMapShards;
        for (auto& shard : m_shards) {
            std::lock_guard lock(shard.mutex);
            shard.map.reserve(perShard);
        }
    }
};
} // namespace orderbook