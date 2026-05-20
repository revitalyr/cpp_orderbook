#pragma once
#include <stdexcept>
#include <memory>
#include "order.h" // Ensure order.h is included
import orderbook.semantic_types; // For F
// TODO add forward_iterator support so that friend class in not needed // Renamed to camelCase
namespace orderbook {

class OrderList {
template <typename TListener> friend class OrderBook;
private: // Internal state
    std::shared_ptr<Order> m_head = nullptr; // Head of the linked list of orders
    std::shared_ptr<Order> m_tail = nullptr; // Tail of the linked list of orders
    orderbook::F m_price;
    Quantity m_totalQuantity{0};
public:
    OrderList(F price) : m_price(price) {}
    const F& price() const { return m_price; }

    Quantity totalQuantity() const noexcept {
        return m_totalQuantity;
    }
    
    struct Iterator 
    {
        friend class OrderList;
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::shared_ptr<Order>;
        using reference         = std::shared_ptr<Order>&;
        
        value_type operator*() const { 
            return current; 
        }
        
        // Prefix increment
        Iterator& operator++() { 
            current = current->m_nextList; 
            return *this;  
        }  
        
        bool operator== (const Iterator& other) const { 
            return current == other.current; 
        }
        
        operator bool() const { 
            return current != nullptr; 
        }
        
    private:
        Iterator(std::shared_ptr<Order> node) : current(node) {}
        std::shared_ptr<Order> current;
    };
    
    void pushBack(std::shared_ptr<Order> order) {
        if (!order) return;
        
        order->m_onList = true;
        m_totalQuantity += order->remainingQuantity();
        
        if (m_head == nullptr) {
            m_head = order;
            m_tail = order;
        } else {
            order->m_prevList = m_tail;
            m_tail->m_nextList = order;
            m_tail = order;
        }
    }
    
    void remove(std::shared_ptr<Order> order) {
        if (!order) return;
        
        if (!order->m_onList) {
            throw std::runtime_error("node is null on removal");
        }
        
        order->m_onList = false;
        m_totalQuantity -= order->remainingQuantity();
        
        auto prev = order->m_prevList.lock();
        auto next = order->m_nextList;

        if (prev) {
            prev->m_nextList = next;
        } else if (m_head == order) {
            m_head = next;
        }

        if (next) {
            next->m_prevList = prev;
        } else if (m_tail == order) {
            m_tail = prev;
        }
        
        // Clear links
        order->m_nextList = nullptr;
        order->m_prevList.reset();
    }
    
    std::shared_ptr<Order> front() const {
        return m_head;
    }
    
    Iterator begin() const { 
        return Iterator(m_head);
    }
    
    Iterator end() const { 
        return Iterator(nullptr);
    }
};

} // namespace orderbook