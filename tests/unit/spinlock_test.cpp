#include <catch2/catch_test_macros.hpp>

#include <thread>
#include <mutex> // For std::lock_guard
#include <vector>
import orderbook;

using namespace orderbook;

TEST_CASE("SpinLock basic operations", "[spinlock]") {
    SpinLock lock;

    {
        orderbook::Guard guard(lock);
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
        orderbook::Guard guard(lock);

        auto fn = [&]() {
            for (int i = 0; i < 1000000; i++) {
                {
                    orderbook::Guard guard_inner(lock);
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
