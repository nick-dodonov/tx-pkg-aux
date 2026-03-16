#pragma once
#include "UpdateCtx.h"
#include <boost/intrusive/list.hpp>
#include <ranges>

namespace App::Loop
{
    class IRunner;

    /// Base loop handler
    class IHandler
    {
    public:
        virtual ~IHandler() = default;
        virtual bool Start() { return true; }
        virtual void Stop() {}
        virtual void Update(const UpdateCtx& ctx) = 0;
    };

    /// Concept for handler-like types
    template <typename T>
    concept HandlerLike = 
        std::derived_from<T, IHandler>
        || 
        requires(T t) {
            { *t } -> std::derived_from<IHandler>;
        };

    /// Wrapper to adapt handler-like types to IHandler
    template <HandlerLike T>
    struct WrapHandler: IHandler
    {
        WrapHandler(T inner)
            : _inner(std::move(inner))
        {}
        bool Start() override { return get().Start(); }
        void Stop() override { get().Stop(); }
        void Update(const UpdateCtx& ctx) override { get().Update(ctx); }

    private:
        T _inner;
        decltype(auto) get()
        {
            if constexpr (requires { *_inner; }) {
                return *_inner;
            } else {
                return _inner;
            }
        }
    };

    /// Typed handler for specific runner type
    class Handler
        : public IHandler
        , public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>
    {
    public:
        using List = boost::intrusive::list<Handler, boost::intrusive::constant_time_size<false>>;

        [[nodiscard]] const IRunner* GetRunner() const { return _runner; }
        [[nodiscard]] IRunner* GetRunner() { return _runner; }

    private:
        friend class Runner;
        void SetRunner(IRunner* runner) { _runner = runner; }

        IRunner* _runner{};
    };

    /// Composite handler that forwards calls to multiple handlers
    struct CompositeHandler: Handler
    {
        void Add(Handler& handler)
        {
            _handlers.push_back(handler);
            if (_running) {
                handler.Start();
            }
        }

        void Remove(Handler& handler)
        {
            auto it = _handlers.iterator_to(handler);
            if (it == _handlers.end()) {
                return;
            }
            if (_running) {
                handler.Stop();
            }
            _handlers.erase(it);
        }

        bool Start() override
        {
            for (auto it = _handlers.begin(); it != _handlers.end(); ++it) {
                if (!it->Start()) {
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

        void Stop() override
        {
            _running = false;
            for (auto& handler : std::ranges::reverse_view(_handlers)) {
                handler.Stop();
            }
        }

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
