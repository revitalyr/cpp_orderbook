module;
#include <chrono>
#include <cfloat>
#include <cstddef>
#include "fixed.h" // For Price type

export module orderbook.constants;

import orderbook.semantic_types; // Import semantic types as constants might use them

export namespace orderbook {

// ============================================================================
// BUFFER AND COLLECTION LIMITS
// ============================================================================

/** Number of shards for the OrderMap to reduce lock contention */
constexpr size_t kOrderMapShards = 16;

/** Default initial capacity for the OrderMap */
constexpr size_t kDefaultOrderMapCapacity = 100000;

/** Maximum number of instrument order books that can be stored */
constexpr size_t kMaxInstruments = 1024;

// Cache-line aligned node size for memory pools
constexpr size_t kNodeSize = 128; // Increased to accommodate std::shared_ptr control blocks

// ============================================================================
// CIRCUIT BREAKER CONFIGURATION
// ============================================================================

/** Maximum number of failures before circuit breaker opens */
constexpr int kFailureThreshold = 10;

/** Time interval during which circuit breaker remains open */
constexpr std::chrono::seconds kCooldownPeriod{30};

/** Interval after which the recursion depth counter is reset */
constexpr std::chrono::seconds kResetInterval{60};

/** Maximum allowed recursion depth to prevent stack overflow */
constexpr int kMaxRecursionDepth = 100;

/** Maximum recursion depth before warning is issued */
constexpr int kWarningThreshold = 70;

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

} // namespace orderbook

// Error Messages
export namespace orderbook::EngineConstants { // Moved into orderbook namespace
constexpr std::string_view kRecursionDepthExceeded = "Recursion depth limit exceeded in insertOrder";
constexpr std::string_view kOrderPointerNull = "Order pointer is null";
constexpr std::string_view kInvalidOrderQuantity = "Invalid order quantity: ";
constexpr std::string_view kOrderAlreadyOnList = "Order is already on a list";
constexpr std::string_view kOrderNotFound = "Order ID not found";
constexpr std::string_view kOrderCannotBeNull = "Order cannot be null";
constexpr std::string_view kExchangeErrorPrefix = "[EXCHANGE ERROR] ";
constexpr std::string_view kOrderInsertionFailed = "Order insertion failed: ";
constexpr std::string_view kExceptionInInsertOrder = "Exception in insertOrder: ";
constexpr std::string_view kOrderbookErrorPrefix = "[ORDERBOOK ERROR] ";
constexpr std::string_view kPriceLevelDoesNotExist = "price level for order does not exist";

// Test-specific constants
constexpr std::string_view kTestSessionId = "session";
constexpr std::string_view kTestOrderIdPrefix = "order_";
constexpr std::string_view kTestQuoteId = "quote1";
constexpr std::string_view kTestInstrumentAAPL = "AAPL";
constexpr std::string_view kTestInstrumentGOOG = "GOOG";
constexpr std::string_view kTestInstrumentMSFT = "MSFT";
} // namespace EngineConstants