#pragma once

/**
 * Global Constants for Trading Engine
 *
 * Consolidates all hardcoded constants used throughout the codebase.
 * Enables easy configuration and prevents magic numbers in code.
 */

#include <chrono>
#include <cfloat>
#include <cstddef>
#include "semantic_types.h"
#include "fixed.h" // For Price type

// ============================================================================
// BUFFER AND COLLECTION LIMITS
// ============================================================================

/** Number of shards for the OrderMap to reduce lock contention */
constexpr size_t kOrderMapShards = 16;

/** Default initial capacity for the OrderMap */
constexpr size_t kDefaultOrderMapCapacity = 100000;

/** Maximum number of instrument order books that can be stored */
constexpr size_t kMaxInstruments = 1024;

/** Cache-line aligned node size for memory pools */
constexpr size_t kNodeSize = 128; // Increased to accommodate std::shared_ptr control blocks

// ============================================================================
// RECURSION AND STACK PROTECTION
// ============================================================================

/** Maximum allowed recursion depth to prevent stack overflow */
constexpr int kMaxRecursionDepth = 50;

/** Maximum recursion depth before warning is issued */
constexpr int kWarningThreshold = 80;

// ============================================================================
// CIRCUIT BREAKER CONFIGURATION
// ============================================================================

/** Maximum number of failures before circuit breaker opens */
constexpr int kFailureThreshold = 10;

/** Time interval during which circuit breaker remains open */
constexpr std::chrono::seconds kCooldownPeriod{30};

/** Interval after which the recursion depth counter is reset */
constexpr std::chrono::seconds kResetInterval{60};

// ============================================================================
// DEFAULT VALUES
// ============================================================================

/** Default instrument symbol for testing */
constexpr const char* kDefaultInstrument = "SYM1";

/** Price value representing market buy (large finite value for fixed-point safety) */
inline const Price kMarketBuyPrice = Price(1000000000);

/** Price value representing market sell (large negative value for fixed-point safety) */
inline const Price kMarketSellPrice = Price(-1000000000);

// ============================================================================
// STRING INTERNER CONSTANTS
// ============================================================================

/** Invalid string ID for the string interner - indicates no string is interned */
constexpr uint32_t kInvalidStringId = 0;

/** Initial capacity for the string interner string vector */
constexpr size_t kInitialStringInternerCapacity = 1024;

// ============================================================================
// MEMORY POOL CONSTANTS
// ============================================================================

/** Number of objects per block in the memory pool */
constexpr size_t kMemoryPoolBlockSize = 4096;
