#pragma once

#include <chrono>
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include "fixed.h"

namespace orderbook {

// ============================================================================
// NUMERIC TYPE ALIASES - Trading Domain
// ============================================================================

/** Unique identifier for an exchange-level order */
using ExchangeId = int64_t;

/** Unique identifier for a trade execution */
using ExecutionId = int64_t;

/** Represents the quantity of units in an order (shares, contracts, etc.) */
using Quantity = int32_t;

/** Represents the number of objects in a collection */
using ObjectCount = size_t;

/** Represents a count of recursive operations or function calls */
using RecursionDepth = int32_t;

/** Represents a failure count in circuit breaker logic */
using FailureCount = int32_t;

// ============================================================================
// STRING TYPE ALIASES - Trading Domain
// ============================================================================

/** Session identifier - uniquely identifies a trading session */
using SessionId = std::string;

/** Session identifier view - for parameters */
using SessionIdView = std::string_view;

/** Instrument symbol (e.g., "AAPL", "EUR/USD") */
using InstrumentSymbol = std::string;

/** Instrument symbol view - for parameters */
using InstrumentSymbolView = std::string_view;

/** Order identifier string representation */
using OrderIdStr = std::string;

/** Order identifier string view - for parameters */
using OrderIdStrView = std::string_view;

/** Quote identifier for multi-leg quotes */
using QuoteId = std::string;

/** Quote identifier view - for parameters */
using QuoteIdView = std::string_view;

// ============================================================================
// TIME TYPE ALIASES - Trading Domain
// ============================================================================

/** Timestamp representing a point in time */
using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

/** Duration representing a time interval */
using Duration = std::chrono::duration<int64_t, std::milli>;

/** Nanosecond-precision timestamp for execution metrics */
using NanosecondTimestamp = int64_t;

// ============================================================================
// PRICE ALIASES
// ============================================================================

/** Semantic alias for price representation using fixed-point arithmetic */
using Price = Fixed<7>;

} // namespace orderbook