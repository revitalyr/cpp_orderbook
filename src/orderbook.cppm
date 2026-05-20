module;

// Глобальный фрагмент модуля для системных заголовков
#include <vector>
#include <memory>
#include <optional>
#include <compare>
#include <atomic>
#include <expected>
#include <ranges>
#include <map>
#include <functional>
#include <iostream>
#include <format>
#include <source_location>
#include <algorithm>
#include <deque>
#include <list>
#include <variant>
#include <sstream>
#include <charconv>
#include <shared_mutex>
#include <thread>

// External headers containing standard includes must be in the global fragment
export module orderbook;

// Import dependent modules
import orderbook.semantic_types;
import orderbook.constants;

// Include headers
#include "core/spinlock.h"
#include "core/memory_pool.h"
#include "core/string_interner.h"
#include "core/insert_result.h"
#include "core/order.h"
#include "core/orderlist.h"
#include "core/pricelevels.h"
#include "core/orderbook.h"
#include "core/bookmap.h"
#include "core/exchange.h"

// Export the orderbook namespace
export namespace orderbook {
    // The headers already define everything in the orderbook namespace
    // We just need to make the namespace available to importers
}
