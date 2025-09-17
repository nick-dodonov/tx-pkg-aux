#include "Log.h"
#include <lwlog.h>

namespace Log
{
    void DefaultInit()
    {
        // using logger_config = lwlog::configuration<
        //     lwlog::disable_local_time,
        //     lwlog::disable_thread_id,
        //     lwlog::disable_process_id,
        //     lwlog::disable_topics
        // >;

        // auto console = std::make_shared<
        //     // lwlog::logger<
        //     // 	logger_config,
        //     // 	lwlog::asynchronous_policy<
        //     // 		lwlog::default_async_queue_size,
        //     // 		lwlog::default_overflow_policy
        //     // 	>,
        //     // 	lwlog::immediate_flush_policy,
        //     // 	lwlog::single_threaded_policy,
        //     // 	lwlog::sinks::stdout_sink
        //     // >
        //     lwlog::basic_logger<
        //         lwlog::sinks::stdout_sink
        //     >
        // >("CONSOLE");

        auto console = std::make_shared<
            lwlog::logger<
                lwlog::default_memory_buffer_limits,
                lwlog::asynchronous_policy<
                    lwlog::default_overflow_policy,
                    lwlog::default_async_queue_size,
                    lwlog::default_thread_affinity
                >,
                lwlog::immediate_flush_policy,
                lwlog::single_threaded_policy,
                lwlog::sinks::stdout_sink
            >
        >("CONSOLE");

        console->set_level_filter(lwlog::level::info | lwlog::level::debug | lwlog::level::critical);
        // console->set_pattern("[%T] [%n] [%l]: %v");
        console->set_pattern("{file} .red([%T] [%n]) .dark_green([:^12{level}]): .cyan(%v) TEXT");
        console->info("Log.DefaultInit");
    }
}
