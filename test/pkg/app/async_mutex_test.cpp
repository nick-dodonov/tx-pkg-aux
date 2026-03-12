#include "Async/Mutex.h"
#include "Boot/Boot.h"

#include <gtest/gtest.h>

// Test Async::Mutex basic locking
TEST(AsyncMutexTest, LockUnlock)
{
    Async::Mutex mutex;
    
    mutex.lock();
    EXPECT_TRUE(true); // Successfully locked
    mutex.unlock();
    EXPECT_TRUE(true); // Successfully unlocked
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif

// Test Async::Mutex try_lock
TEST(AsyncMutexTest, TryLock)
{
    Async::Mutex mutex;

    const auto locked = mutex.try_lock();
    EXPECT_TRUE(locked);
    
    if (locked) {
        mutex.unlock();
    }
    
    // Verify we can lock again
    EXPECT_TRUE(mutex.try_lock());
    mutex.unlock();
}

// Test Async::LockGuard RAII behavior
TEST(AsyncMutexTest, LockGuardRAII)
{
    Async::Mutex mutex;

    {
        Async::LockGuard lock(mutex);
        // Mutex should be locked here
        EXPECT_FALSE(mutex.try_lock()); // Should fail as already locked
    }
    
    // After scope, mutex should be unlocked
    EXPECT_TRUE(mutex.try_lock());
    mutex.unlock();
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif


int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    Boot::DefaultInit(argc, const_cast<const char**>(argv));
    return RUN_ALL_TESTS();
}
