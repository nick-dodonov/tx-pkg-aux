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

        // Простая демонстрация создания logger для разных версий lwlog
        // Версия 1.4.0: используем basic_logger (без registry)
        // Версия 1.3.1: используем basic_logger + registry
        
        auto console = std::make_shared<
            lwlog::basic_logger<lwlog::sinks::stdout_sink>
        >("CONSOLE");
        
        // Пытаемся использовать registry, если он есть (версия 1.3.1)
        // Если нет - просто логируем (версия 1.4.0)
        #ifdef LWLOG_HAS_REGISTRY
            lwlog::registry::instance().register_logger(console.get());
        #endif
        
        console->info("Log.DefaultInit - lwlog version compatibility demonstrated!");
    }
}
