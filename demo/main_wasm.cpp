#include "simple_log.h"
#include <emscripten/bind.h>

int main() {
    simple_log::info("WASM Demo started successfully!");
    simple_log::info("System initialized");
    simple_log::error("This is a test error message");
    return 0;
}

// Экспорт функций для JavaScript
EMSCRIPTEN_BINDINGS(simple_log_demo) {
    emscripten::function("info", &simple_log::info);
    emscripten::function("error", &simple_log::error);
}