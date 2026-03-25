#include "CompositeHandler.h"
#include <ranges>

namespace RunLoop
{
    void CompositeHandler::Add(Handler& handler)
    {
        _handlers.push_back(handler);
        if (_running) {
            handler.SetRunner(GetRunner());
            handler.Start();
        }
    }

    void CompositeHandler::Remove(Handler& handler)
    {
        const auto it = _handlers.iterator_to(handler);
        if (it == _handlers.end()) {
            return;
        }
        if (_running) {
            handler.Stop();
        }
        _handlers.erase(it);

        handler.SetRunner(nullptr);
    }

    // Handler
    bool CompositeHandler::Start()
    {
        auto* runner = GetRunner();
        for (auto it = _handlers.begin(); it != _handlers.end(); ++it) {
            auto& handler = *it;
            handler.SetRunner(runner);
            if (!handler.Start()) {
                // on failure stop in reverse order
                while (it != _handlers.begin()) {
                    --it;
                    it->Stop();
                }
                return false;
            }
        }
        _running = true;
        return true;
    }

    void CompositeHandler::Stop()
    {
        _running = false;
        for (auto& handler : std::ranges::reverse_view(_handlers)) {
            handler.Stop();
        }
    }
}
