#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "fixed.h"
#include "semantic_types.h"

namespace orderbook {

// ============================================================================
// BUFFER AND COLLECTION LIMITS
// ============================================================================

constexpr size_t kOrderMapShards = 16;
constexpr size_t kDefaultOrderMapCapacity = 100000;
constexpr size_t kMaxInstruments = 1024;
constexpr size_t kNodeSize = 128;

// ============================================================================
// CIRCUIT BREAKER CONFIGURATION
// ============================================================================

constexpr int kFailureThreshold = 10;
constexpr std::chrono::seconds kCooldownPeriod{30};
constexpr std::chrono::seconds kResetInterval{60};
constexpr int kMaxRecursionDepth = 100;
constexpr int kWarningThreshold = 70;

// ============================================================================
// DEFAULT VALUES
// ============================================================================

inline constexpr const char* kDefaultInstrument = "SYM1";
inline const Price kMarketBuyPrice = Price(1000000000);
inline const Price kMarketSellPrice = Price(-1000000000);

// ============================================================================
// STRING INTERNER CONSTANTS
// ============================================================================

constexpr uint32_t kInvalidStringId = 0;
constexpr size_t kInitialStringInternerCapacity = 1024;

// ============================================================================
// MEMORY POOL CONSTANTS
// ============================================================================

constexpr size_t kMemoryPoolBlockSize = 4096;

} // namespace orderbook
