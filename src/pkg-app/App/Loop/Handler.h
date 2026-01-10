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

    /// Typed handler for specific runner type
    template <typename TRunner>
    class Handler: public IHandler
    {
    public:
        using Runner = TRunner;

        virtual bool Started(TRunner& runner) { return true; }
        virtual void Stopping(TRunner& runner) {}
        virtual bool Update(TRunner& runner, const UpdateCtx& ctx) = 0;

    private:
        bool Started(IRunner& runner) override { return this->Started(static_cast<TRunner&>(runner)); }
        void Stopping(IRunner& runner) override { this->Stopping(static_cast<TRunner&>(runner)); }
        bool Update(IRunner& runner, const UpdateCtx& ctx) override { return this->Update(static_cast<TRunner&>(runner), ctx); }
    };
}
