#pragma once
#include "UpdateCtx.h"

namespace App::Loop
{
    class IRunner;

    /// Base loop handler
    class IHandler
    {
    public:
        virtual ~IHandler() = default;
        virtual bool Started(IRunner& runner) { return true; }
        virtual void Stopping(IRunner& runner) {}
        virtual bool Update(IRunner& runner, const UpdateCtx& ctx) = 0;
    };

    template <typename T>
    struct WrapHandler: IHandler
    {
        WrapHandler(T inner)
            : _inner(std::move(inner))
        {}
        bool Started(IRunner& runner) override { return get().Started(runner); }
        void Stopping(IRunner& runner) override { get().Stopping(runner); }
        bool Update(IRunner& runner, const UpdateCtx& ctx) override { return get().Update(runner, ctx); }

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
    class Handler: public IHandler
    {
    private:
        friend class Runner;
        void SetParent(IRunner* runner) { _runner = runner; }

        IRunner* _runner;
    };
}
