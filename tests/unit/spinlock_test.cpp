#include <catch2/catch_test_macros.hpp>

#include <thread>
#include <vector>
#include "core/exchange.h" // For ExchangeListener

#include "core/spinlock.h"

TEST_CASE("SpinLock basic operations", "[spinlock]") {
    SpinLock lock;

    {
        Guard guard(lock);
        REQUIRE_FALSE(lock.tryLock());
        REQUIRE(lock.isLocked());
    }
    REQUIRE_FALSE(lock.isLocked());
    REQUIRE(lock.tryLock());
    lock.unlock();
    REQUIRE(lock.tryLock());
}

TEST_CASE("SpinLock multithreaded", "[spinlock]") {
    SpinLock lock;

    std::vector<std::thread> threads;

    long count = 0;

    {
        Guard guard(lock);

        auto fn = [&]() {
            for (int i = 0; i < 1000000; i++) {
                {
                    Guard guard_inner(lock);
                    count++;
                }
            }
        };

        threads.push_back(std::thread(fn));
        threads.push_back(std::thread(fn));
    }

    for (auto thread = threads.begin(); thread != threads.end(); thread++) {
        thread->join();
    }

    REQUIRE(count == 2000000);
}
