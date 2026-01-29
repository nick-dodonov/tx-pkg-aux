#include <emscripten.h>
#include <string>

EM_JS(int, is_node, (), {
    return typeof process !== 'undefined' && 
           process.versions && 
           process.versions.node;
});

EM_JS(int, is_browser, (), {
    return typeof window !== 'undefined' && 
           typeof document !== 'undefined';
});

//TODO: test w/ different -sENVIRONMENT=web,node
// EM_JS(int, is_running_in_node, (), {
//     return ENVIRONMENT_IS_NODE;
// });
// EM_JS(int, is_running_in_web, (), {
//     return ENVIRONMENT_IS_WEB;
// });
//TODO: test workers too
// EM_JS(int, get_environment, (), {
//     if (typeof process !== 'undefined' && 
//         process.versions && 
//         process.versions.node) {
//         return 0; // ENV_NODE
//     }
//     if (typeof window !== 'undefined' && 
//         typeof document !== 'undefined') {
//         return 1; // ENV_BROWSER
//     }
//     if (typeof importScripts === 'function') {
//         return 2; // ENV_WORKER
//     }
//     return 3; // ENV_UNKNOWN
// });


//TODO: test $UTF8ToString(url) $lengthBytesUTF8(txt) $stringToUTF8(txt, ptr, len) w/o -sEXPORTED_RUNTIME_METHODS=lengthBytesUTF8,stringToUTF8,UTF8ToString
EM_ASYNC_JS(char*, fetch_impl, (const char* url), {
    const response = await fetch(UTF8ToString(url));
    const text = await response.text();
    const len = lengthBytesUTF8(text) + 1;
    const ptr = _malloc(len);
    stringToUTF8(text, ptr, len);
    return ptr;
});

std::string makeHttpRequest(const std::string& url) {
    char* result = fetch_impl(url.c_str());
    if (!result) {
        return {};
    }
    std::string response(result);
    free(result);
    return response;
}

int main() {
    if (is_node()) {
        printf("Running in Node.js\n");
    }
    if (is_browser()) {
        printf("Running in Browser\n");
    }
    
    std::string data = makeHttpRequest("https://jsonplaceholder.typicode.com/todos/1");
    printf("Response:\n%s\n", data.c_str());

    // workaround to properly exit in browser environment
    if (is_browser()) {
        exit(0);
        //emscripten_force_exit(0);
    }
    return 0;
}

// #include <emscripten/fetch.h>
// #include <stdio.h>
// #include <string.h>

// void onSuccess(emscripten_fetch_t* fetch) {
//     printf("Downloaded %llu bytes\n", fetch->numBytes);
//     printf("Data: %.*s\n", (int)fetch->numBytes, fetch->data);
//     emscripten_fetch_close(fetch);
    
//     // Продолжаем выполнение после получения данных
//     // Можете вызвать другую функцию отсюда
// }

// void onError(emscripten_fetch_t* fetch) {
//     printf("Download failed: %d\n", fetch->status);
//     emscripten_fetch_close(fetch);
// }

// int main() {
//     emscripten_fetch_attr_t attr;
//     emscripten_fetch_attr_init(&attr);
//     strcpy(attr.requestMethod, "GET");
//     attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
//     attr.onsuccess = onSuccess;
//     attr.onerror = onError;
    
//     emscripten_fetch(&attr, "https://example.com/");
    
//     // main() завершается немедленно, но callbacks будут вызваны позже
//     return 0;
// }