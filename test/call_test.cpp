#include <benchmark/benchmark.h>
#include <functional>
#include <vector>

// Интерфейс
class ICaller {
public:
    virtual ~ICaller() = default;
    virtual int call(int x) = 0;
};

// Реализация интерфейса
class Caller : public ICaller {
public:
    int call(int x) override {
        return x + 1;
    }
};

// Функция для std::function
int free_call(int x) {
    return x + 1;
}

static void BM_VirtualInterfaceCall(benchmark::State& state) {
    Caller caller_impl;
    ICaller* interface_ptr = &caller_impl;
    int sum = 0;
    int i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(sum += interface_ptr->call(i++));
    }
}
BENCHMARK(BM_VirtualInterfaceCall);

static void BM_StdFunctionFreeCall(benchmark::State& state) {
    std::function<int(int)> std_func = free_call;
    int sum = 0;
    int i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(sum += std_func(i++));
    }
}
BENCHMARK(BM_StdFunctionFreeCall);

static void BM_StdFunctionLambdaCall(benchmark::State& state) {
    std::function<int(int)> std_func = [](int x) { return x + 1; };
    int sum = 0;
    int i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(sum += std_func(i++));
    }
}
BENCHMARK(BM_StdFunctionLambdaCall);

BENCHMARK_MAIN();
