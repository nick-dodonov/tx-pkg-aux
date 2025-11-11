//#include "Http/SimpleClient.h"
#include "Log/Log.h"
#include <asio.hpp>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

int main()
{
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    Log::InfoF("[{}] Test SimpleClient stub", oss.view());

    asio::steady_timer timer(asio::system_executor(), 5s);
    timer.async_wait([](const std::error_code& ec)
    {
        Log::InfoF("Hello, Timer! {}", ec.message());
    });

    std::thread thread([](){
        for (int i = 0; i <= 100; ++i) {
            asio::post([i](){
                std::ostringstream oss;
                oss << std::this_thread::get_id();
                std::cout << std::format("[{}] Thread: {}\n", oss.view(), i);
            });
        }
    });
    thread.join();

    asio::detail::global<asio::system_context>().join();

    Log::InfoF("[{}] Exiting", oss.view());
    return 0;
}
