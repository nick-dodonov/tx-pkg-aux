#include "Cross/Defines.h"
#include <benchmark/benchmark.h>
#include <functional>

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
    int call(int x) override { return x + 1; }
};

class Caller2: public ICaller
{
public:
    int call(int x) override { return x + 2; }
};

// Factory to prevent devirtualization
CROSS_NOINLINE ICaller* create_caller(int type)
{
    if (type == 1) {
        return new Caller1();
    }
    return new Caller2();
}

// Functions for std::function
int free_call1(int x)
{
    return x + 1;
}

int free_call2(int x)
{
    return x + 2;
}

static void BM_VirtualInterfaceCall(benchmark::State& state)
{
    ICaller* caller1 = create_caller(1);
    ICaller* caller2 = create_caller(2);

    int sum = 0;
    int i = 0;
    for (auto _ : state) {
        // Alternate calls to prevent branch prediction and devirtualization
        benchmark::DoNotOptimize(sum += (i % 2 == 0 ? caller1 : caller2)->call(i));
        i++;
    }

    delete caller1;
    delete caller2;
}
BENCHMARK(BM_VirtualInterfaceCall);

static void BM_StdFunctionFreeCall(benchmark::State& state)
{
    std::function<int(int)> func1 = free_call1;
    std::function<int(int)> func2 = free_call2;

    int sum = 0;
    int i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(sum += (i % 2 == 0 ? func1 : func2)(i));
        i++;
    }
}
BENCHMARK(BM_StdFunctionFreeCall);

static void BM_StdFunctionLambdaCall(benchmark::State& state)
{
    std::function<int(int)> func1 = [](int x) { return x + 1; };
    std::function<int(int)> func2 = [](int x) { return x + 2; };

    int sum = 0;
    int i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(sum += (i % 2 == 0 ? func1 : func2)(i));
        i++;
    }
}
BENCHMARK(BM_StdFunctionLambdaCall);

BENCHMARK_MAIN();
