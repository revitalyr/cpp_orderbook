#pragma once

#include <deque>
#include <map>
#include <functional>
#include <memory>
#include "order.h"
#include "orderlist.h"

// The purpose of PriceLevels is to allow a compile time indirection to test implementation using different data structures

struct price_compare {
    explicit price_compare(bool ascending) : m_ascending(ascending) {} // Renamed to camelCase
    template<class T, class U>
    inline bool operator()(const T& t, const U& u) const {
        return (m_ascending) ? t.price() < u : t.price() > u;
    }
    const bool m_ascending;
};

struct PriceCompareStruct { // Renamed to PascalCase
    explicit PriceCompareStruct(bool ascending) : m_ascending(ascending) {}
    template<class T, class U>
    inline bool operator()(const T& t, const U& u) const {
        return (m_ascending) ? t.price() < u : t.price() > u;
    }
    const bool m_ascending;
};

template <typename Container>
concept ContainerOfPtr = requires(Container c) {
    typename Container::value_type;
    requires std::same_as<typename Container::value_type, std::shared_ptr<OrderList>>;
};

template <typename Container>
concept ContainerOfStruct = requires(Container c) {
    typename Container::value_type;
    requires std::same_as<typename Container::value_type, OrderList>;
};

template <typename Container>
concept MapOfStruct = requires(Container c) {
    typename Container::value_type;
    requires std::same_as<typename Container::value_type, std::pair<Price,OrderList>>;
};

template <typename Container>
concept MapOfPtr = requires(Container c) {
    typename Container::value_type;
    requires std::same_as<typename Container::value_type, std::pair<Price,std::shared_ptr<OrderList>>>;
};

template <typename ContainerOfPtr>
class PointerPriceLevels {
private: // Internal state
    const price_compare m_cmpFn; // Comparison function for prices // Renamed to camelCase
    ContainerOfPtr m_levels; // Container holding order lists // Renamed to camelCase
public: // Public interface
    PointerPriceLevels(bool ascending) : m_cmpFn(ascending) {}
    void insertOrder(std::shared_ptr<Order> order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order->price(), m_cmpFn);
        std::shared_ptr<OrderList> list;
        if (itr == m_levels.end() || (*itr)->price() != order->price()) {
            list = std::make_shared<OrderList>(order->price());
            m_levels.insert(itr, list);
        } else {
            list = *itr;
        }
        list->pushBack(order);
    }
    void removeOrder(std::shared_ptr<Order> order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order->price(), m_cmpFn);
        if (itr == m_levels.end() || (*itr)->price() != order->price()) {
            throw std::runtime_error("price level for order does not exist");
        }
        auto list = *itr;
        list->remove(order);
        if (list->front() == nullptr) {
            m_levels.erase(itr);
        }
    }
    std::shared_ptr<Order> front() const {
        auto itr = m_levels.begin();
        return itr == m_levels.end() ? nullptr : (*itr)->front();
    }
    size_t size() const {
        return m_levels.size();
    }
    void forEach(std::function<void(const OrderList*)> fn) const {
        for(auto itr=m_levels.begin();itr!=m_levels.end();itr++) {
            fn(*itr);
        }
    }
};

template <typename ContainerOfStruct>
class StructPriceLevels {
private: // Internal state
    const PriceCompareStruct m_cmpFn; // Comparison function for prices // Renamed to camelCase
    ContainerOfStruct m_levels; // Container holding order lists // Renamed to camelCase
public: // Public interface
    StructPriceLevels(bool ascending) : m_cmpFn(ascending) {}
    void insertOrder(std::shared_ptr<Order> order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order->price(), m_cmpFn);
        if (itr == m_levels.end() || itr->price() != order->price()) {
            OrderList list(order->price());
            list.pushBack(order);
            m_levels.insert(itr, std::move(list));
        } else {
            itr->pushBack(order);
        }
    }
    void removeOrder(std::shared_ptr<Order> order) {
        auto itr = std::lower_bound(m_levels.begin(), m_levels.end(), order->price(), m_cmpFn);
        if (itr == m_levels.end() || itr->price() != order->price()) {
            throw std::runtime_error("price level for order does not exist");
        }
        itr->remove(order);
        if (itr->front() == nullptr) {
            m_levels.erase(itr);
        }
    }
    std::shared_ptr<Order> front() const {
        auto itr = m_levels.begin();
        if (itr == m_levels.end()) return nullptr;
        return itr->front();
    }
    bool empty() const {
        return m_levels.empty();
    }
    size_t size() const {
        return m_levels.size();
    }
    void forEach(std::function<void(const OrderList*)> fn) const {
        for (auto itr = m_levels.begin(); itr != m_levels.end(); itr++) {
            fn(&(*itr));
        }
    }
};

struct fixed_compare {
    explicit fixed_compare(bool ascending) : m_ascending(ascending) {} // Renamed to camelCase
    bool operator()(const Price& t, const Price& u) const {
        return (m_ascending) ? t < u : t > u;
    }
    const bool m_ascending; // Renamed to m_snake_case
};

template <typename MapOfStruct>
class MapPriceLevels {
private:
    const fixed_compare m_cmpFn;
    MapOfStruct m_levels;
public: // Public interface
    MapPriceLevels(bool ascending) : m_cmpFn(ascending), m_levels(m_cmpFn) {} 
    void insertOrder(std::shared_ptr<Order> order) {
        auto itr = m_levels.lower_bound(order->price());
        if (itr == m_levels.end() || itr->first != order->price()) {
            OrderList list(order->price());
            list.pushBack(order);
            m_levels.insert({list.price(), std::move(list)});
        } else {
            itr->second.pushBack(order);
        }
    }
    void removeOrder(std::shared_ptr<Order> order) { // Renamed to camelCase
        auto itr = m_levels.lower_bound(order->price());
        if (itr == m_levels.end() || itr->first != order->price()) {
            throw std::runtime_error("price level for order does not exist");
        }
        itr->second.remove(order);
        if (itr->second.front() == nullptr) {
            m_levels.erase(itr);
        }
    }
    std::shared_ptr<Order> front() const {
        auto itr = m_levels.begin();
        if (itr == m_levels.end()) return nullptr;
        return itr->second.front();
    }
    bool empty() const {
        return m_levels.empty();
    }
    size_t size() const {
        return m_levels.size();
    }
    void forEach(std::function<void(const OrderList*)> fn) const {
        for (auto itr = m_levels.begin(); itr != m_levels.end(); itr++) {
            fn(&(itr->second));
        }
    }
};

template <typename MapOfPtr>
class MapPtrPriceLevels {
private:
    const fixed_compare m_cmpFn; // Comparison function for prices // Renamed to camelCase
    MapOfPtr m_levels; // Container holding order lists // Renamed to camelCase
public: // Public interface
    MapPtrPriceLevels(bool ascending) : m_cmpFn(ascending), m_levels(m_cmpFn) {} // Renamed to m_snake_case
    void insertOrder(std::shared_ptr<Order> order) {
        auto itr = m_levels.lower_bound(order->price());
        if (itr == m_levels.end() || itr->first != order->price()) {
            auto list = std::make_shared<OrderList>(order->price());
            list->pushBack(order);
            m_levels.insert({list->price(), list});
        } else {
            itr->second->pushBack(order);
        }
    }
    void removeOrder(std::shared_ptr<Order> order) { // Renamed to camelCase
        auto itr = m_levels.lower_bound(order->price());
        if (itr == m_levels.end() || itr->first != order->price()) {
            throw std::runtime_error("price level for order does not exist");
        }
        itr->second->remove(order);
        if (itr->second->front() == nullptr) {
            m_levels.erase(itr);
        }
    }
    std::shared_ptr<Order> front() const {
        auto itr = m_levels.begin();
        if (itr == m_levels.end()) return nullptr;
        return itr->second->front();
    }
    bool empty() const {
        return m_levels.empty();
    }
    size_t size() const {
        return m_levels.size();
    }
    void forEach(std::function<void(const OrderList*)> fn) const {
        for (auto itr = m_levels.begin(); itr != m_levels.end(); itr++) {
            fn(itr->second.get());
        }
    }
};


using DequeuePtrPriceLevels = PointerPriceLevels<std::deque<std::shared_ptr<OrderList>>>;
using VectorPointerPriceLevels = PointerPriceLevels<std::vector<std::shared_ptr<OrderList>>>;
using VectorPriceLevels = StructPriceLevels<std::vector<OrderList>>;
using StdMapPriceLevels = MapPriceLevels<std::map<Price, OrderList, fixed_compare>>;
using StdMapPointerPriceLevels = MapPtrPriceLevels<std::map<Price, std::shared_ptr<OrderList>, fixed_compare>>;

// define the PriceLevels implementation to use
using PriceLevels = VectorPriceLevels;