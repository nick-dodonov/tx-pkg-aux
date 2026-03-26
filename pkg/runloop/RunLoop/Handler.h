#pragma once
#include "UpdateCtx.h"
#include <boost/intrusive/list.hpp>

namespace RunLoop
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
        using HandlerList = boost::intrusive::list<Handler, boost::intrusive::constant_time_size<false>>;

        [[nodiscard]] const IRunner* GetRunner() const { return _runner; }
        [[nodiscard]] IRunner* GetRunner() { return _runner; }

    private:
        friend class Runner;
        friend class CompositeHandler;
        void SetRunner(IRunner* runner) { _runner = runner; }

        IRunner* _runner{};
    };
}
