#pragma once
#include <stdexcept>
#include <boost/intrusive/list.hpp>
#include "order.h"

namespace orderbook {

namespace bi = boost::intrusive;

/**
 * @brief Intrusive doubly-linked list of Orders at a single price level.
 *
 * Orders are linked through a `boost::intrusive::list_member_hook<>` embedded
 * in the `Order` struct.  The list does **not** own the Orders — lifetime is
 * managed externally (e.g. via `OrderMap`).
 *
 * Copy is prohibited because the intrusive chain would be shared between
 * copies, leading to double-free; move is safe and noexcept.
 */
class OrderList {
    template <typename TListener> friend class OrderBook;
private:
    using Hook = bi::list_member_hook<bi::link_mode<bi::normal_link>>;
    using MemberHook = bi::member_hook<Order, Hook, &Order::m_listHook>;
    using List = bi::list<Order, MemberHook, bi::constant_time_size<false>>;

    List m_list;
    Price m_price;
    Quantity m_totalQuantity{0};

public:
    OrderList(Price price) : m_price(price) {}

    OrderList(OrderList&&) noexcept = default;
    OrderList& operator=(OrderList&&) noexcept = default;

    OrderList(const OrderList&) = delete;
    OrderList& operator=(const OrderList&) = delete;

    /// Intrusive list does not own elements — trivial destructor.
    ~OrderList() = default;

    const Price& price() const { return m_price; }
    Quantity totalQuantity() const noexcept { return m_totalQuantity; }

    /**
     * @brief Append @p order at the end of the list.
     *
     * The caller must ensure `order` is not already on any list.
     */
    void pushBack(Order& order) {
        order.m_onList = true;
        m_totalQuantity += order.remainingQuantity();
        m_list.push_back(order);
    }

    /**
     * @brief Remove @p order from the list.
     * @throws std::runtime_error if the order is not on any list.
     */
    void remove(Order& order) {
        if (!order.m_onList)
            throw std::runtime_error("node is null on removal");
        order.m_onList = false;
        m_totalQuantity -= order.remainingQuantity();
        m_list.erase(List::s_iterator_to(order));
    }

    Order* front() {
        return m_list.empty() ? nullptr : &m_list.front();
    }

    const Order* front() const {
        return m_list.empty() ? nullptr : &m_list.front();
    }

    using Iterator = List::const_iterator;

    Iterator begin() const { return m_list.begin(); }
    Iterator end() const   { return m_list.end(); }
};

} // namespace orderbook
