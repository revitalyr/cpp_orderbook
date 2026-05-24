# C++20 Modules Migration Plan — cpp_orderbook

> Based on full include‑graph analysis of all 18 headers and 6 source files.

---

## 1. Current State Summary

### Include Graph (DAG — no cycles)

```
semantic_types.h ──┐
constants.h ───────┤
spinlock.h ────────┤
memory_pool.h ─────┤
string_interner.h ─┤
order.h ───────────┼──► orderbook.cppm (includes ALL)
orderlist.h ───────┤
insert_result.h ───┤
pricelevels.h ─────┤
ordermap.h ────────┤
orderbook.h ───────┤
bookmap.h ─────────┤
exchange.h ────────┘
     │
     └──► safety/production_safety.h
```

Each arrow is a `#include`. Some headers also emit `import orderbook.*` in parallel — a **hybrid anti‑pattern** that defeats module benefits.

### Rebuild Hot Spots (descending)

| File | TUs that transitively include it |
|---|---|
| `external/cpp_fixed/fixed.h` | **16+** (every TU) |
| `include/core/constants.h` | 14+ |
| `include/core/order.h` | 13+ |
| `include/core/spinlock.h` | 12+ |
| `include/core/orderbook.h` | 10+ |
| `include/core/semantic_types.h` | 7+ |

### Dead / Unused Files

- `src/exchange.cpp` — not in any CMake target; non‑template `.cpp` cannot compile `template<typename TListener> class Exchange`
- `src/orderbook.cpp` — empty (1 blank line)
- `src/production_safety_inline.h / .cpp` — duplicate of `include/safety/production_safety.h`

---

## 2. Target Module Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   orderbook (primary)                    │
│  re‑exports all partitions, provides glue templates     │
├─────────────────────────────────────────────────────────┤
│  orderbook.exchange        (partition)                  │
│  orderbook.bookmap         (partition)                  │
│  orderbook.orderbook       (partition)                  │
│  orderbook.pricelevels     (partition)                  │
│  orderbook.insert_result   (partition)                  │
│  orderbook.ordermap        (partition)                  │
│  orderbook.orderlist       (partition)                  │
│  orderbook.order           (partition)                  │
│  orderbook.memory_pool     (partition)                  │
│  orderbook.string_interner (partition)                  │
│  orderbook.spinlock        (partition)                  │
│  orderbook.production_safety (partition)                │
├─────────────────────────────────────────────────────────┤
│  orderbook.constants        (submodule)                 │
│  orderbook.semantic_types   (submodule)                 │
└─────────────────────────────────────────────────────────┘
```

Dependency direction: `semantic_types ← constants ← everything else`

---

## 3. Migration Phases

### Phase 0 — Prep (1 day)
| Step | Action |
|------|--------|
| 0.1 | Remove dead files: `src/exchange.cpp`, `src/orderbook.cpp`, `src/production_safety_inline.h/.cpp` |
| 0.2 | Deduplicate `ProductionSafety` — keep `include/safety/production_safety.h`, remove `include/core/production_safety.h` |
| 0.3 | Strip all `import orderbook.*` from every `.h` file (prevent hybrid anti‑pattern) |
| 0.4 | Verify `git diff` — no behavioural changes, only file deletion |

### Phase 1 — Leaf modules (already done)

| Module | Status | File |
|--------|--------|------|
| `orderbook.semantic_types` | **DONE** — `src/semantic_types.cppm` |
| `orderbook.constants` | **DONE** — `src/constants.cppm` |

### Phase 2 — Low‑level partitions (2–3 days)

These headers depend only on modules from Phase 1 and/or stdlib.

| Header → | Module Partition | .cppm file |
|-----------|-----------------|------------|
| `spinlock.h` | `orderbook.spinlock` | `src/spinlock.cppm` |
| `constants.h` | already a module | — |
| `string_interner.h` | `orderbook.string_interner` | `src/string_interner.cppm` |
| `memory_pool.h` | `orderbook.memory_pool` | `src/memory_pool.cppm` |
| `semantic_types.h` | already a module | — |
| `production_safety.h` | `orderbook.production_safety` | `src/production_safety.cppm` |

Each partition exports the `orderbook` namespace contents from its header. Example:

```cpp
// src/spinlock.cppm
module;
#include <atomic>
#include <mutex>
export module orderbook:spinlock;
export namespace orderbook { /* SpinLock, Guard */ }
```

### Phase 3 — Mid‑level partitions (2–3 days)

| Header → | Module Partition | Imports |
|----------|-----------------|---------|
| `order.h` | `orderbook:order` | `:string_interner`, `:memory_pool`, `orderbook.semantic_types`, `orderbook.constants` |
| `orderlist.h` | `orderbook:orderlist` | `:order` |
| `insert_result.h` | `orderbook:insert_result` | `orderbook.semantic_types`, `orderbook.constants` |
| `ordermap.h` | `orderbook:ordermap` | `:order`, `:spinlock`, `orderbook.constants` |

### Phase 4 — High‑level partitions (2–3 days)

| Header → | Module Partition | Imports |
|----------|-----------------|---------|
| `pricelevels.h` | `orderbook:pricelevels` | `:order`, `:orderlist`, `orderbook.semantic_types` |
| `orderbook.h` | `orderbook:orderbook` | `:spinlock`, `:pricelevels`, `:insert_result`, `:production_safety`, `orderbook.semantic_types`, `orderbook.constants` |
| `bookmap.h` | `orderbook:bookmap` | `:orderbook`, `orderbook.constants` |
| `exchange.h` | `orderbook:exchange` | `:order`, `:orderbook`, `:bookmap`, `:spinlock`, `:ordermap`, `orderbook.semantic_types`, `orderbook.constants` |

### Phase 5 — Primary module interface unit (1 day)

Rewrite `src/orderbook.cppm`:

```cpp
module;
// global fragment: stdlib headers only
export module orderbook;

// import submodules
import orderbook.semantic_types;
import orderbook.constants;

// import partitions
import :spinlock;
import :string_interner;
import :memory_pool;
import :production_safety;
import :order;
import :orderlist;
import :insert_result;
import :ordermap;
import :pricelevels;
import :orderbook;
import :bookmap;
import :exchange;

// re‑export everything
export using namespace orderbook;
```

### Phase 6 — Drop legacy headers (1 day)

| Action | Rationale |
|--------|-----------|
| Remove `#pragma once` headers from `include/core/` | All consumers switch to `import orderbook;` |
| Remove `include_directories(...)` from CMake | No longer needed |
| Keep `include/safety/production_safety.h` as a thin forwarding header | If downstream needs header-only inclusion |

### Phase 7 — Migrate tests (1 day)

All test TUs currently using `#include "core/orderbook.h"` → change to `import orderbook;`. Examples like `comprehensive_demo.cpp` already use pure `import orderbook;` — extend this to all test/benchmark files.

---

## 4. ABI / API Compatibility Strategy

| Concern | Strategy |
|---------|----------|
| **ABI** | All `orderbook::` types remain in `namespace orderbook` with identical layout. Modules impose no ABI break vs headers on MSVC/Clang. |
| **API** | Public API is identical. Every name exported from the module was already in a header. |
| **Forward declarations** | Replace forward decls via `#include` with `import` of the owning partition. |
| **Macros** | `PRODUCTION_CRITICAL_GUARD` / `PRODUCTION_CIRCUIT_BREAKER` stay in the module TU; no macro leakage across `import`. |

---

## 5. Build Time Impact (projected)

| Metric | Before | After | Δ |
|--------|--------|-------|---|
| `fixed.h` inclusions | 16+ TUs | **1 TU** (global fragment of `semantic_types.cppm`) | −94 % |
| `order.h` reparses | 13+ TUs | **1 partition** (then `import`) | −92 % |
| `orderbook.h` reparses | 10+ TUs | **1 partition** | −90 % |
| Full rebuild | baseline | ~2–3× slower (module BMI serialization) | |
| Incremental rebuild | baseline | **~5–10× faster** (change one partition → rebuild 1 BMI, not N headers) | |

**Net**: full rebuilds get slower (BMI cost), **incremental rebuilds become dramatically faster** — the key win for a trading engine with rapid iteration cycles.

---

## 6. Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| **Compiler support gaps** (GCC module support incomplete) | Target MSVC 17.10+ and Clang 18+. Pin C++23 (`CMAKE_CXX_STANDARD 23`). Use `CMAKE_CXX_FLAGS` fallback for GCC. |
| **BMI cache invalidation** | Use `CXX_MODULE_SKIP_DEPENDENCY_CACHE` only in CI. Keep `CXX_MODULES` file‑set in CMake. |
| **Circular partition imports** | Preserve the DAG invariant: `semantic_types ← constants ← spinlock ← string_interner ← memory_pool ← order ← orderlist ← insert_result ← ordermap ← pricelevels ← orderbook ← bookmap ← exchange`. Never import a partition that would create a cycle. |
| **Template instantiation across module boundary** | All `OrderBook<T>`, `Exchange<T>` are templates in headers; moving to partitions requires explicit instantiation or keeping in the primary module TU. **Recommend**: keep all template definitions in the primary `orderbook.cppm` (which re‑exports): |

```cpp
// primary module interface unit
export import :orderbook;   // OrderBook<T> declared here
export import :exchange;    // Exchange<T> declared here
// Their definitions stay in the partition .cpp — fine for MSVC/Clang.
```
