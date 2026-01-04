#pragma once
#include <functional>
#include "UpdateCtx.h"

namespace App::Loop
{
    /// Context passed to the looper when updating is finished.
    /// Used to signal exit code or other finalization options 
    /// in several looper implementations that cannot exit from the synchronous Start().
    struct FinishData
    {
        explicit FinishData(const int exitCode)
            : ExitCode(exitCode)
        {}

        int ExitCode{};
    };

    ////////////////////////////////////////////////
    class ILooper;

    template<typename TLooper>
    class Looper;

    /// Base loop handler
    class IHandler
    {
    public:
        virtual ~IHandler() = default;
        virtual bool Update(ILooper& looper, const UpdateCtx& ctx) = 0;
    };

    /// Base looper that runs the update action while the handler returns true
    class ILooper : public std::enable_shared_from_this<ILooper>
    {
    public:
        virtual ~ILooper() = default;

        using HandlerPtr = std::unique_ptr<IHandler>;
        virtual void Start(HandlerPtr handler) = 0;
        virtual void Finish(const FinishData& finishData) = 0;
    };

    /// Back-compatibility handler TODO: rm
    class FuncHandler : public IHandler
    {
    public:
        using UpdateAction = std::function<bool(const UpdateCtx&)>;
        explicit FuncHandler(UpdateAction&& updateAction) : _updateAction{std::move(updateAction)} {}

        bool Update(ILooper& looper, const UpdateCtx& ctx) override { return _updateAction(ctx); }

    private:
        UpdateAction _updateAction;
    };

    // template<typename TLooper>
    // class Handler : public IHandler
    // {
    // public:
    //     virtual bool Update(Looper<TLooper>& looper, const UpdateCtx& ctx) = 0;
    //
    // protected:
    //     bool Update(ILooper& looper, const UpdateCtx& ctx) override
    //     {
    //         static_assert(std::is_base_of_v<Looper<TLooper>, TLooper>);
    //         return static_cast<Looper<TLooper>&>(looper).Update(ctx);
    //     }
    // };
    //
    // template<typename TLooper>
    // class Looper : public ILooper
    // {
    // public:
    //     virtual void Start(UpdateAction updateAction) = 0;
    //
    // };
    //
    // struct MyLooper : Looper<MyLooper>
    // {
    //
    // };
}
