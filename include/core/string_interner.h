#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <memory>
#include "constants.h"

namespace orderbook {

/**
 * String interning for frequently used strings.
 * Reduces memory by storing unique strings once and using 32-bit IDs.
 */
class StringInterner {
public:
    using StringId = uint32_t;
    static constexpr StringId INVALID_ID = 0;

private:
    struct StringData {
        std::unique_ptr<char[]> data;
        size_t length;
        size_t hash;
    };

    std::vector<StringData> m_strings;  // Index 0 reserved for INVALID_ID
    std::unordered_map<size_t, std::vector<StringId>> m_hashToIds;
    mutable std::shared_mutex m_mutex;

    static size_t compute_hash(std::string_view sv) {
        // FNV-1a hash
        size_t hash = 14695981039346656037ull;
        for (char c : sv) {
            hash ^= static_cast<size_t>(static_cast<unsigned char>(c));
            hash *= 1099511628211ull;
        }
        return hash;
    }

public:
    StringInterner() {
        m_strings.reserve(kInitialStringInternerCapacity);
        m_strings.push_back({nullptr, 0, 0}); // INVALID_ID
    }

    /**
     * Intern a string and return its ID.
     * Thread-safe.
     */
    StringId intern(std::string_view sv) {
        if (sv.empty()) return INVALID_ID;

        size_t hash = compute_hash(sv);
        
        // Extreme Fast path: thread-local cache to avoid ANY mutex contention
        thread_local std::unordered_map<size_t, StringId> tl_cache;
        auto cache_it = tl_cache.find(hash);
        if (cache_it != tl_cache.end()) return cache_it->second;

        // Fast path: check if already exists (shared read lock)
        {
            std::shared_lock lock(m_mutex);
            auto it = m_hashToIds.find(hash);
            if (it != m_hashToIds.end()) {
                for (StringId id : it->second) {
                    const auto& str = m_strings[id];
                    if (str.length == sv.length() && 
                        std::memcmp(str.data.get(), sv.data(), sv.length()) == 0) {
                        tl_cache[hash] = id;
                        return id;
                    }
                }
            }
        }

        // Slow path: insert new string (write lock)
        std::unique_lock lock(m_mutex);
        
        // Double-check after acquiring write lock
        auto it = m_hashToIds.find(hash);
        if (it != m_hashToIds.end()) {
            for (StringId id : it->second) {
                const auto& str = m_strings[id];
                if (str.length == sv.length() && 
                    std::memcmp(str.data.get(), sv.data(), sv.length()) == 0) {
                    return id;
                }
            }
        }

        // Insert new string
        StringId new_id = static_cast<StringId>(m_strings.size()); // Renamed to m_snake_case
        
        auto buffer = std::make_unique<char[]>(sv.length() + 1);
        std::memcpy(buffer.get(), sv.data(), sv.length());
        buffer[sv.length()] = '\0';
        
        m_strings.push_back({std::move(buffer), sv.length(), hash}); // Renamed to m_snake_case
        m_hashToIds[hash].push_back(new_id); // Renamed to m_snake_case
        
        tl_cache[hash] = new_id;
        return new_id;
    }

    /**
     * Get string by ID.
     * Returns empty string_view for INVALID_ID.
     */
    std::string_view get(StringId id) const {
        if (id == INVALID_ID || id >= m_strings.size()) { // Renamed to m_snake_case
            return {};
        }
        
        std::shared_lock lock(m_mutex); // Renamed to m_snake_case
        const auto& str = m_strings[id]; // Renamed to m_snake_case
        return std::string_view(str.data.get(), str.length);
    }

    size_t size() const {
        std::shared_lock lock(m_mutex); // Renamed to m_snake_case
        return m_strings.size() - 1; // Exclude INVALID_ID // Renamed to m_snake_case
    }

    void reserve(size_t n) {
        std::unique_lock lock(m_mutex); // Renamed to m_snake_case
        m_strings.reserve(n + 1); // Renamed to m_snake_case
    }
};

/**
 * Global string interner instance
 */
inline StringInterner& g_globalStringInterner() { // Renamed to g_camelCase
    static StringInterner interner;
    return interner;
}

} // namespace orderbook
