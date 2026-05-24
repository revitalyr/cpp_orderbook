#pragma once

#include <deque>
#include <functional>
#include <map>
#include <vector>
#include "order.h"
#include "orderlist.h"

namespace orderbook {

/**
 * @brief Bidirectional price comparator.
 *
 * Returns `t < u` when ascending, `t > u` when descending.
 * Handles both `OrderList`/`Entry` objects (via `.price()`) and raw `Price` values.
 */
struct price_compare {
    explicit price_compare(bool ascending) : m_ascending(ascending) {}

    template<class T, class U>
    inline bool operator()(const T& t, const U& u) const {
        if constexpr (requires { t.price(); })
            return m_ascending ? t.price() < u : t.price() > u;
        else
            return m_ascending ? t < u : t > u;
    }

    const bool m_ascending;
};

// ---------------------------------------------------------------------------
// Compile-time concepts for the various PriceLevels backends
// ---------------------------------------------------------------------------

template <typename Container>
concept ContainerOfPtr = requires(Container c) {
    typename Container::value_type;
    requires std::same_as<typename Container::value_type, std::shared_ptr<orderbook::OrderList>>;
};

template <typename Container>
concept ContainerOfStruct = requires(Container c) {
    typename Container::value_type;
    requires std::same_as<typename Container::value_type, orderbook::OrderList>;
};

template <typename Container>
concept MapOfStruct = requires(Container c) {
    typename Container::value_type;
    requires std::same_as<typename Container::value_type, std::pair<Price,orderbook::OrderList>>;
};

template <typename Container>
concept MapOfPtr = requires(Container c) {
    typename Container::value_type;
    requires std::same_as<typename Container::value_type, std::pair<Price,std::shared_ptr<OrderList>>>;
};

// ---------------------------------------------------------------------------
// PriceLevels implementations — each satisfies the same duck-typed interface.
// `using PriceLevels = …` at the bottom selects the active one.
// ---------------------------------------------------------------------------

/**
 * @brief Backed by a sorted container of `shared_ptr<OrderList>`.
 */
template <typename ContainerOfPtr>
class PointerPriceLevels {
    const price_compare m_cmpFn;
    ContainerOfPtr m_levels;
public:
    PointerPriceLevels(bool ascending) : m_cmpFn(ascending) {}
    void insertOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(), m_cmpFn);
        std::shared_ptr<OrderList> list;
        if (itr == m_levels.end() || (*itr)->price() != order.price()) {
            list = std::make_shared<OrderList>(order.price());
            m_levels.insert(itr, list);
        } else {
            list = *itr;
        }
        list->pushBack(order);
    }
    void removeOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(), m_cmpFn);
        if (itr == m_levels.end() || (*itr)->price() != order.price())
            throw std::runtime_error("price level for order does not exist");
        auto list = *itr;
        list->remove(order);
        if (list->front() == nullptr) m_levels.erase(itr);
    }
    Order* front() const {
        auto itr = m_levels.begin();
        return itr == m_levels.end() ? nullptr : (*itr)->front();
    }
    size_t size() const { return m_levels.size(); }
    void forEach(std::function<void(const OrderList*)> fn) const {
        for (auto itr = m_levels.begin(); itr != m_levels.end(); ++itr) fn(*itr);
    }
};

/**
 * @brief Backed by a sorted container of `OrderList` (value semantics).
 */
template <typename ContainerOfStruct>
class StructPriceLevels {
    const price_compare m_cmpFn;
    ContainerOfStruct m_levels;
public:
    StructPriceLevels(bool ascending) : m_cmpFn(ascending) {}
    void insertOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(), m_cmpFn);
        if (itr == m_levels.end() || itr->price() != order.price()) {
            OrderList list(order.price());
            list.pushBack(order);
            m_levels.insert(itr, std::move(list));
        } else {
            itr->pushBack(order);
        }
    }
    void removeOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(), m_cmpFn);
        if (itr == m_levels.end() || itr->price() != order.price())
            throw std::runtime_error("price level for order does not exist");
        itr->remove(order);
        if (itr->front() == nullptr) m_levels.erase(itr);
    }
    Order* front() const {
        auto itr = m_levels.begin();
        return itr == m_levels.end() ? nullptr : itr->front();
    }
    bool empty() const { return m_levels.empty(); }
    size_t size() const { return m_levels.size(); }
    void forEach(std::function<void(const OrderList*)> fn) const {
        for (auto itr = m_levels.begin(); itr != m_levels.end(); ++itr) fn(&(*itr));
    }
};

/**
 * @brief Backed by a `std::map<Price, OrderList, price_compare>` (value semantics).
 */
template <typename MapOfStruct>
class MapPriceLevels {
    const price_compare m_cmpFn;
    MapOfStruct m_levels;
public:
    MapPriceLevels(bool ascending) : m_cmpFn(ascending), m_levels(m_cmpFn) {}
    void insertOrder(Order& order) {
        auto itr = m_levels.lower_bound(order.price());
        if (itr == m_levels.end() || itr->first != order.price()) {
            OrderList list(order.price());
            list.pushBack(order);
            m_levels.insert({list.price(), std::move(list)});
        } else {
            itr->second.pushBack(order);
        }
    }
    void removeOrder(Order& order) {
        auto itr = m_levels.lower_bound(order.price());
        if (itr == m_levels.end() || itr->first != order.price())
            throw std::runtime_error("price level for order does not exist");
        itr->second.remove(order);
        if (itr->second.front() == nullptr) m_levels.erase(itr);
    }
    Order* front() const {
        auto itr = m_levels.begin();
        return itr == m_levels.end() ? nullptr : itr->second.front();
    }
    bool empty() const { return m_levels.empty(); }
    size_t size() const { return m_levels.size(); }
    void forEach(std::function<void(const OrderList*)> fn) const {
        for (auto itr = m_levels.begin(); itr != m_levels.end(); ++itr) fn(&(itr->second));
    }
};

/**
 * @brief Backed by a `std::map<Price, shared_ptr<OrderList>, price_compare>`.
 */
template <typename MapOfPtr>
class MapPtrPriceLevels {
    const price_compare m_cmpFn;
    MapOfPtr m_levels;
public:
    MapPtrPriceLevels(bool ascending) : m_cmpFn(ascending), m_levels(m_cmpFn) {}
    void insertOrder(Order& order) {
        auto itr = m_levels.lower_bound(order.price());
        if (itr == m_levels.end() || itr->first != order.price()) {
            auto list = std::make_shared<OrderList>(order.price());
            list->pushBack(order);
            m_levels.insert({list->price(), list});
        } else {
            itr->second->pushBack(order);
        }
    }
    void removeOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(), m_cmpFn);
        if (itr == m_levels.end() || itr->first != order.price())
            throw std::runtime_error("price level for order does not exist");
        itr->second->remove(order);
        if (itr->second->front() == nullptr) m_levels.erase(itr);
    }
    Order* front() const {
        auto itr = m_levels.begin();
        return itr == m_levels.end() ? nullptr : itr->second->front();
    }
    bool empty() const { return m_levels.empty(); }
    const MapOfPtr& levels() const { return m_levels; }
    size_t size() const { return m_levels.size(); }
    void forEach(std::function<void(const OrderList*)> fn) const {
        for (auto itr = m_levels.begin(); itr != m_levels.end(); ++itr)
            fn(itr->second.get());
    }
};

/**
 * @brief Backed by a sorted `vector<Entry>` with pool-allocated `OrderList` objects.
 *
 * OrderLists are allocated from a static `MemoryPool<OrderList>` singleton.
 * The destructor returns every live OrderList to the pool. This avoids per‑level
 * heap allocations and is significantly faster than `std::map` in single‑threaded
 * use — but it is **not** thread‑safe (vector reallocation invalidates iterators).
 */
class PooledPriceLevels {
public:
    struct Entry {
        Price m_price;
        OrderList* m_orders;

        const Price& price() const { return m_price; }
        Quantity totalQuantity() const { return m_orders->totalQuantity(); }
    };

private:
    std::vector<Entry> m_levels;
    price_compare m_cmpFn;

    static MemoryPool<OrderList>& pool() {
        static MemoryPool<OrderList> p;
        return p;
    }

public:
    PooledPriceLevels(bool ascending) : m_cmpFn(ascending) {
        pool().reserve(256);
    }

    /// Returns every live OrderList to the pool.
    ~PooledPriceLevels() {
        for (auto& entry : m_levels)
            pool().deallocate(entry.m_orders);
    }

    void insertOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(),
            [this](const Entry& e, Price p) { return m_cmpFn(e.m_price, p); });

        if (itr == m_levels.end() || itr->m_price != order.price()) {
            auto* list = pool().construct(order.price());
            if (!list) {
                pool().reserve(256);
                list = pool().construct(order.price());
            }
            if (!list) std::abort();
            list->pushBack(order);
            m_levels.insert(itr, {order.price(), list});
        } else {
            itr->m_orders->pushBack(order);
        }
    }

    void removeOrder(Order& order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order.price(),
            [this](const Entry& e, Price p) { return m_cmpFn(e.m_price, p); });

        if (itr == m_levels.end() || itr->m_price != order.price())
            throw std::runtime_error("price level for order does not exist");

        itr->m_orders->remove(order);
        if (itr->m_orders->front() == nullptr) {
            pool().deallocate(itr->m_orders);
            m_levels.erase(itr);
        }
    }

    Order* front() const {
        if (m_levels.empty()) return nullptr;
        return m_levels.front().m_orders->front();
    }

    bool empty() const { return m_levels.empty(); }
    size_t size() const { return m_levels.size(); }

    void forEach(std::function<void(const OrderList*)> fn) const {
        for (const auto& entry : m_levels)
            fn(entry.m_orders);
    }

    const std::vector<Entry>& levels() const { return m_levels; }
};

// ---------------------------------------------------------------------------
// Named aliases for each implementation
// ---------------------------------------------------------------------------

using DequeuePtrPriceLevels    = PointerPriceLevels<std::deque<std::shared_ptr<OrderList>>>;
using VectorPointerPriceLevels = PointerPriceLevels<std::vector<std::shared_ptr<OrderList>>>;
using VectorPriceLevels        = StructPriceLevels<std::vector<OrderList>>;
using StdMapPriceLevels        = MapPriceLevels<std::map<Price, OrderList, price_compare>>;
using StdMapPointerPriceLevels = MapPtrPriceLevels<std::map<Price, std::shared_ptr<OrderList>, price_compare>>;

/// Active PriceLevels implementation — override this typedef to switch backends.
using PriceLevels = PooledPriceLevels;

} // namespace orderbook
