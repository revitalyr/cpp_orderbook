#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <vector>

#include "spinlock.h"

namespace orderbook { struct Order; }

namespace orderbook {

constexpr size_t kMemoryPoolBlockSize = 4096;
constexpr size_t kNodeSize = 128;

/// Suppress MSVC warning about intentional 64-byte padding for cache-line alignment.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

/**
 * @brief Lock-free fixed-size memory pool with intrusive free list.
 *
 * Allocates memory in blocks of `BlockSize` PoolNodes. Each node is 64-byte
 * aligned for cache-line isolation. Thread-safe via SpinLock on push/pop.
 *
 * @tparam T        The object type stored in the pool.
 * @tparam BlockSize Number of nodes per allocation block.
 */
template<typename T, size_t BlockSize = kMemoryPoolBlockSize>
class MemoryPool {
public:
    /// Minimum node size — max(sizeof(T), kNodeSize).
    static constexpr size_t ActualNodeSize = (sizeof(T) > kNodeSize) ? sizeof(T) : kNodeSize;

    /**
     * @brief A single pool node — cache-line aligned storage + intrusive next pointer.
     */
    struct PoolNode {
        alignas(64) char object_storage[ActualNodeSize];
        std::atomic<PoolNode*> next{nullptr};

        T* object() { return reinterpret_cast<T*>(object_storage); }
    };

private:
    /// A contiguous block of PoolNodes (64-byte aligned so every node is aligned).
    struct alignas(64) Block {
        char m_data[BlockSize * sizeof(PoolNode)];
        Block* m_next{nullptr};
    };

    mutable SpinLock m_lock;
    std::vector<std::unique_ptr<Block>> m_blocks;
    PoolNode* m_freeList{nullptr};
    std::atomic<size_t> m_allocatedCount{0};
    std::atomic<size_t> m_capacity{0};

public:
    MemoryPool() = default;
    ~MemoryPool() = default;

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    /**
     * @brief Pre-allocate enough blocks to guarantee at least @p n free nodes.
     *
     * The free count is computed as `capacity - allocatedCount`. This prevents
     * short-circuiting when total capacity is high but all nodes are in use.
     *
     * @param n Minimum number of nodes that must be available after the call.
     */
    void reserve(size_t n) {
        std::lock_guard lock(m_lock);

        size_t freeCount = m_capacity.load(std::memory_order_relaxed)
                         - m_allocatedCount.load(std::memory_order_relaxed);
        if (n <= freeCount) return;

        size_t toAllocate = n - freeCount;
        size_t blocks_needed = (toAllocate + BlockSize - 1) / BlockSize;

        for (size_t b = 0; b < blocks_needed; ++b) {
            auto block = std::make_unique<Block>();
            PoolNode* nodes = reinterpret_cast<PoolNode*>(block->m_data);

            for (size_t i = 0; i < BlockSize - 1; ++i)
                nodes[i].next.store(&nodes[i + 1], std::memory_order_relaxed);
            nodes[BlockSize - 1].next.store(nullptr, std::memory_order_relaxed);

            nodes[BlockSize - 1].next.store(m_freeList, std::memory_order_relaxed);
            m_freeList = &nodes[0];

            m_blocks.push_back(std::move(block));
        }

        m_capacity.fetch_add(blocks_needed * BlockSize, std::memory_order_relaxed);
    }

    /**
     * @brief Pop a free node, placement-new nothing.
     * @return Pointer to the node, or nullptr if exhausted.
     */
    T* allocate() {
        PoolNode* node = popFree();
        if (!node) return nullptr;
        m_allocatedCount.fetch_add(1, std::memory_order_relaxed);
        return reinterpret_cast<T*>(node);
    }

    /**
     * @brief Destruct @p ptr and return its node to the free list.
     */
    void deallocate(T* ptr) {
        if (!ptr) return;
        ptr->~T();
        PoolNode* node = reinterpret_cast<PoolNode*>(ptr);
        pushFree(node);
        m_allocatedCount.fetch_sub(1, std::memory_order_relaxed);
    }

    /**
     * @brief Allocate + placement-new in one call.
     */
    template <typename... Args>
    T* construct(Args&&... args) {
        T* ptr = allocate();
        if (ptr)
            new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }

    size_t allocated_count() const {
        return m_allocatedCount.load(std::memory_order_relaxed);
    }

    size_t capacity() const {
        return m_capacity.load(std::memory_order_relaxed);
    }

    void recordAlloc() { m_allocatedCount.fetch_add(1, std::memory_order_relaxed); }
    void recordDealloc() { m_allocatedCount.fetch_sub(1, std::memory_order_relaxed); }

public:
    /// @name Low-level operations — exposed for MemoryPoolAllocator.
    PoolNode* popFree() {
        std::lock_guard lock(m_lock);
        if (!m_freeList) return nullptr;
        PoolNode* node = m_freeList;
        m_freeList = node->next.load(std::memory_order_relaxed);
        return node;
    }

    void pushFree(PoolNode* node) {
        std::lock_guard lock(m_lock);
        node->next.store(m_freeList, std::memory_order_relaxed);
        m_freeList = node;
    }
};

/**
 * @brief STL-compatible allocator wrapping a MemoryPool singleton.
 *
 * Enables `std::allocate_shared<T>` to allocate from a lock-free pool instead
 * of the heap. Use via a wrapper class with a static `instance()` method:
 * @code
 *   using Alloc = MemoryPoolAllocator<Order, OrderPool>;
 *   auto order = std::allocate_shared<Order>(Alloc{}, args...);
 * @endcode
 *
 * @tparam T        Value type (the object to allocate).
 * @tparam PoolType Class exposing `static MemoryPool<T>& instance()`.
 */
template <typename T, typename PoolType>
struct MemoryPoolAllocator {
    using value_type = T;

    MemoryPoolAllocator() = default;
    template <typename U>
    MemoryPoolAllocator(const MemoryPoolAllocator<U, PoolType>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n != 1) throw std::bad_array_new_length();
        auto* node = PoolType::instance().popFree();
        if (node) [[likely]] {
            PoolType::instance().recordAlloc();
            return reinterpret_cast<T*>(node);
        }
        PoolType::instance().reserve(256);
        auto* r = PoolType::instance().popFree();
        if (r) PoolType::instance().recordAlloc();
        return reinterpret_cast<T*>(r);
    }

    void deallocate(T* p, std::size_t) noexcept {
        PoolType::instance().pushFree(
            reinterpret_cast<decltype(PoolType::instance().popFree())>(p));
        PoolType::instance().recordDealloc();
    }

    bool operator==(const MemoryPoolAllocator&) const = default;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace orderbook
