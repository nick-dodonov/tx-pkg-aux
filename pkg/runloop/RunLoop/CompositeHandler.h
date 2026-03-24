#pragma once
#include "Handler.h"

namespace RunLoop
{
    /// Composite handler that forwards calls to multiple handlers
    class CompositeHandler: public Handler
    {
    public:
        void Add(Handler& handler);
        void Remove(Handler& handler);

        // Handler
        bool Start() override;
        void Stop() override;
        void Update(const UpdateCtx& ctx) override
        {
            for (auto& handler : _handlers) {
                handler.Update(ctx);
            }
        }

    private:
        Handler::List _handlers;
        bool _running{};
    };
}
