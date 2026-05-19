#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

#include "spinlock.h"
#include "constants.h"

struct Order; // Forward declaration in global namespace

namespace orderbook {

/**
 * Suppression for MSVC alignment warnings.
 * Padding is intentional to ensure cache-line (64-byte) alignment.
 */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

/**
 * Lock-free memory pool for fixed-size objects.
 * Uses intrusive linked list for free nodes.
 */
template<typename T, size_t BlockSize = kMemoryPoolBlockSize>
class MemoryPool {
public:
    // Cache-line aligned storage for the object
    static constexpr size_t ActualNodeSize = (sizeof(T) > kNodeSize) ? sizeof(T) : kNodeSize;
    
    struct PoolNode {
        alignas(64) char object_storage[ActualNodeSize]; 
        std::atomic<PoolNode*> next{nullptr};
        
        T* object() { return reinterpret_cast<T*>(object_storage); }
    };

private:
    struct alignas(64) Block { // Represents a block of memory // Renamed to PascalCase
        char m_data[BlockSize * sizeof(PoolNode)]; // Raw memory for nodes // Renamed to m_snake_case
        Block* m_next{nullptr}; // Pointer to the next block in the chain // Renamed to m_snake_case
    };

    mutable SpinLock m_lock;
    std::vector<std::unique_ptr<Block>> m_blocks; // Renamed to m_snake_case
    PoolNode* m_freeList{nullptr};
    std::atomic<size_t> m_allocatedCount{0}; // Renamed to m_snake_case
    std::atomic<size_t> m_capacity{0}; // Renamed to m_snake_case

public:
    MemoryPool() = default;
    ~MemoryPool() = default;

    // Disable copy/move
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    /**
     * Pre-allocate memory blocks
     */
    void reserve(size_t n) {
        std::lock_guard lock(m_lock);
        
        size_t current = m_capacity.load(std::memory_order_relaxed);
        if (n <= current) return;

        size_t blocks_needed = (n - current + BlockSize - 1) / BlockSize;
        
        for (size_t b = 0; b < blocks_needed; ++b) {
            auto block = std::make_unique<Block>(); // Renamed to camelCase
            PoolNode* nodes = reinterpret_cast<PoolNode*>(block->m_data); // Renamed to m_snake_case
            
            // Link nodes in this block
            for (size_t i = 0; i < BlockSize - 1; ++i) {
                nodes[i].next.store(&nodes[i + 1], std::memory_order_relaxed);
            }
            nodes[BlockSize - 1].next.store(nullptr, std::memory_order_relaxed);

            // Add to free list (lock-free push)
            nodes[BlockSize - 1].next.store(m_freeList, std::memory_order_relaxed);
            m_freeList = &nodes[0];

            m_blocks.push_back(std::move(block));
        }

        m_capacity.fetch_add(blocks_needed * BlockSize, std::memory_order_relaxed);
    }

    /**
     * Allocate object from pool
     */
    T* allocate() { // Renamed to camelCase
        PoolNode* node = popFree();
        if (!node) {
            return nullptr; // Pool exhausted
        }
        
        m_allocatedCount.fetch_add(1, std::memory_order_relaxed);
        return reinterpret_cast<T*>(node);
    }

    /**
     * Deallocate object back to pool
     */
    void deallocate(T* ptr) { // Renamed to camelCase
        if (!ptr) return;
        
        // Explicitly call destructor
        ptr->~T();
        
        PoolNode* node = reinterpret_cast<PoolNode*>(ptr);
        pushFree(node);
        
        m_allocatedCount.fetch_sub(1, std::memory_order_relaxed);
    }

    /**
     * Construct object in place
     */
    template <typename... Args>
    T* construct(Args&&... args) {
        T* ptr = allocate();
        if (ptr) {
            new (ptr) T(std::forward<Args>(args)...);
        }
        return ptr;
    }

    size_t allocated_count() const {
        return m_allocatedCount.load(std::memory_order_relaxed); // Renamed to m_snake_case
    }

    size_t capacity() const {
        return m_capacity.load(std::memory_order_relaxed); // Renamed to m_snake_case
    }

private:
    PoolNode* popFree() { // Renamed to camelCase
        std::lock_guard lock(m_lock);
        if (!m_freeList) return nullptr;
        
        PoolNode* node = m_freeList;
        m_freeList = node->next.load(std::memory_order_relaxed);
        return node;
    }

    void pushFree(PoolNode* node) { // Renamed to camelCase
        std::lock_guard lock(m_lock);
        node->next.store(m_freeList, std::memory_order_relaxed);
        m_freeList = node;
    }
};

/**
 * STL-compatible allocator wrapper for MemoryPool.
 * Allows std::allocate_shared to use the pool for zero-heap allocation.
 */
template <typename T, typename PoolType>
struct MemoryPoolAllocator {
    using value_type = T;

    MemoryPoolAllocator() = default;
    template <typename U>
    MemoryPoolAllocator(const MemoryPoolAllocator<U, PoolType>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n != 1) throw std::bad_array_new_length();
        return reinterpret_cast<T*>(PoolType::instance().popFree());
    }

    void deallocate(T* p, std::size_t) noexcept {
        PoolType::instance().pushFree(reinterpret_cast<typename PoolType::PoolNode*>(p));
    }

    bool operator==(const MemoryPoolAllocator&) const = default;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace orderbook
