#pragma once

#include <gtest/gtest.h>

#define CO_ASSERT_TRUE(expr) \
    do { \
        EXPECT_TRUE(expr); \
        if (!(expr)) co_return; \
    } while (false)

#define CO_ASSERT_FALSE(expr) \
    do { \
        EXPECT_FALSE(expr); \
        if (expr) co_return; \
    } while (false)

#define CO_ASSERT_EQ(val1, val2) \
    do { \
        EXPECT_EQ(val1, val2); \
        if (!((val1) == (val2))) co_return; \
    } while (false)

#define CO_ASSERT_NE(val1, val2) \
    do { \
        EXPECT_NE(val1, val2); \
        if (!((val1) != (val2))) co_return; \
    } while (false)

#define CO_FAIL(msg) \
    do { \
        GTEST_FAIL() << msg; \
        co_return; \
    } while (false)
