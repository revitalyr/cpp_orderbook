# 📈 Performance Benchmarks

This document provides comprehensive performance analysis and benchmarks for the C++ Order Book trading engine with smart pointer architecture.

> **Note**: Current benchmarks run in **Debug** configuration (with sanitizers and assertions).  
> Release builds are expected to yield significantly higher throughput.

## 🎯 Executive Summary

The memory pool-backed order book achieves **~214K ops/sec** (Debug, 8-core) for multithreaded order insertion, and **~302K ops/sec** for order cancellation. Performance in Release builds is projected to be 10–50× higher.

## 📊 Benchmark Results

### Hardware Specifications
- **CPU**: Intel Core (8 logical cores)
- **OS**: Windows
- **Compiler**: MSVC 19.50 (Visual Studio 2022)
- **Build**: Debug (`/Od /RTC1 /Zi`) — C++23 (`/std:c++latest`)
- **Memory Pool**: `kMemoryPoolBlockSize=4096`, `kNodeSize=128`

### Current Benchmarks (Debug Build)

| Test | Operations | Time/Op (μs) | Throughput (ops/sec) |
|------|-----------|-------------|-------------------|
| **Multithread Insert (no trades)** | 4M (8 threads) | 4.66 μs | **214,435** |
| **Multithread Insert (with trades)** | 4M (8 threads) | 6.66 μs | **150,044** |
| **Multithread Cancel** | 2M | 3.30 μs | **302,802** |
| **Single-thread Insert (10 levels)** | — | — | Crashes (stack overflow in Debug — 10M pool reserve) |
| **Single-thread Cancel (10 levels)** | — | — | Crashes (stack overflow in Debug — 1M pool reserve) |

> ⚠️ The single-threaded benchmark (`benchmark_test.exe`) crashes with `STATUS_STACK_OVERFLOW (0xC00000FD)` in Debug mode due to reserving 10M pool nodes (~2 GB virtual). Run with a Release build or reduce `kNumOrders`.

### Memory Pool Auto-Reserve

When the pool is empty (no pre-reservation), the `MemoryPoolAllocator` automatically reserves 256 elements to prevent null-pointer crashes. For maximum performance, call `OrderPool::reserve(n)` upfront:

```cpp
orderbook::OrderPool::reserve(expected_order_count);
```

### Comparison: Debug vs Expected Release

| Metric | Debug (current) | Estimated Release | Improvement |
|--------|----------------|-------------------|-------------|
| **Insert (multithread)** | 214K ops/sec | ~5–10M ops/sec | 25–50× |
| **Cancel (multithread)** | 302K ops/sec | ~4–8M ops/sec | 15–25× |

## 🏗️ Current Architecture

### Memory Pool Allocation

Orders are allocated via `MemoryPoolAllocator<T, OrderPool>` which wraps `MemoryPool<Order>` — a fixed-size, pre-reserved arena:

```cpp
using Allocator = MemoryPoolAllocator<Order, OrderPool>;
auto order = std::allocate_shared<Order>(Allocator{}, ...);
```

- **Pre-reserve**: `OrderPool::reserve(n)` — must be called before use for zero-heap allocation
- **Auto-reserve**: If the pool is empty, `MemoryPoolAllocator::allocate` auto-reserves 256 elements as a safety fallback
- **STL-compatible**: Works with `std::allocate_shared`, `std::allocate_shared` for control block + object allocation

### Lock-Free Operations

`BookMap` uses lock-free `std::atomic<shared_ptr>` slots for per-instrument `OrderBook` lookup.
`OrderMap` uses sharded `std::unordered_map` backed by `SpinLock` per shard for concurrent access.
`MemoryPool` uses `SpinLock` on `pushFree`/`popFree` for thread-safe pool management.

## 🧪 Benchmark Methodology

### Benchmark Files

| File | Description |
|------|-------------|
| `tests/benchmark/benchmark_test.cpp` | Single-threaded insert/cancel with configurable price levels; uses 5M buy + 5M sell orders |
| `tests/benchmark/benchmark_multithread_test.cpp` | 8-thread insert/cancel on separate instruments; 250K buys + 250K sells per thread |

### Current Limitations
- Benchmarks run in **Debug** configuration (`/Od /RTC1`) — results are not representative of production
- Single-threaded benchmark (`benchmark_test.exe`) crashes with stack overflow in Debug (10M pool nodes)
- Build in **Release** (`/O2`) to obtain meaningful throughput numbers

### How to Run
```powershell
# Build benchmarks
cmake --build build --target benchmark_multithread_test
# Run
.\build\benchmark_multithread_test.exe
```

## 🔧 Performance Tuning

### Memory Pool Size
The pool block size and node size can be tuned in `include/core/constants.h`:
- `kMemoryPoolBlockSize = 4096` — nodes per allocation block
- `kNodeSize = 128` — minimum node storage (must fit the largest allocated type)

### Debug vs Release
Debug builds include:
- Stack frame checks (`/RTC1`)
- No optimizations (`/Od`)
- Iterator debugging (MSVC STL)

For accurate benchmarks, build with:
```
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

---

## 📋 Status

The benchmark infrastructure is operational. Current Debug-mode results provide a baseline for regression testing. Release-mode benchmarking requires a separate build configuration.
