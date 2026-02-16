#include "Cross/Defines.h"
#include <benchmark/benchmark.h>
#include <functional>

// ReSharper disable CppDFAUnreadVariable

// Interface
class ICaller
{
public:
    virtual ~ICaller() = default;
    virtual int call(int x) = 0;
};

// Interface implementations
class Caller1: public ICaller
{
public:
    int call(const int x) override { return x + 1; }
};

class Caller2: public ICaller
{
public:
    int call(const int x) override { return x + 2; }
};

// Factory to prevent devirtualization
CROSS_NOINLINE ICaller* create_caller(int type)
{
    if (type == 1) {
        return new Caller1(); // NOLINT(*-owning-memory)
    }
    return new Caller2();
}

// Functions for std::function
int free_call1(const int x)
{
    return x + 1;
}

int free_call2(const int x)
{
    return x + 2;
}

static void BM_VirtualInterfaceCall(benchmark::State& state)
{
    auto* caller1 = create_caller(1);
    auto* caller2 = create_caller(2);

    auto sum = 0;
    auto i = 0;
    for (auto _ : state) { // NOLINT(*-deadcode.DeadStores)
        // Alternate calls to prevent branch prediction and devirtualization
        benchmark::DoNotOptimize(sum += (i % 2 == 0 ? caller1 : caller2)->call(i));
        i++;
    }

    delete caller1; // NOLINT(*-owning-memory)
    delete caller2; // NOLINT(*-owning-memory)
}
BENCHMARK(BM_VirtualInterfaceCall);

static void BM_StdFunctionFreeCall(benchmark::State& state)
{
    const std::function func1 = free_call1;
    const std::function func2 = free_call2;

    auto sum = 0;
    auto i = 0;
    for (auto _ : state) { // NOLINT(*-deadcode.DeadStores)
        benchmark::DoNotOptimize(sum += (i % 2 == 0 ? func1 : func2)(i));
        i++;
    }
}
BENCHMARK(BM_StdFunctionFreeCall);

static void BM_StdFunctionLambdaCall(benchmark::State& state)
{
    const std::function func1 = [](const int x) { return x + 1; };
    const std::function func2 = [](const int x) { return x + 2; };

    auto sum = 0;
    auto i = 0;
    for (auto _ : state) { // NOLINT(*-deadcode.DeadStores)
        benchmark::DoNotOptimize(sum += (i % 2 == 0 ? func1 : func2)(i));
        i++;
    }
}
BENCHMARK(BM_StdFunctionLambdaCall);

BENCHMARK_MAIN();
