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
        virtual bool Started() { return true; }
        virtual void Stopping() {}
        virtual void Update(const UpdateCtx& ctx) = 0;
    };

    template <typename T>
    struct WrapHandler: IHandler
    {
        WrapHandler(T inner)
            : _inner(std::move(inner))
        {}
        bool Started() override { return get().Started(); }
        void Stopping() override { get().Stopping(); }
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
    class Handler: public IHandler
    {
    public:
        [[nodiscard]] const IRunner* GetRunner() const { return _runner; }
        [[nodiscard]] IRunner* GetRunner() { return _runner; }

    private:
        friend class Runner;
        void SetRunner(IRunner* runner) { _runner = runner; }

        IRunner* _runner{};
    };
}
