#pragma once
#include <functional>
#include "UpdateCtx.h"

namespace App::Loop
{
    /// Context passed to the runner when updating is finished.
    /// Used to signal exit code or other finalization options 
    /// in several runner implementations that cannot exit from the synchronous Start().
    struct FinishData
    {
        explicit FinishData(const int exitCode)
            : ExitCode(exitCode)
        {}

        int ExitCode{};
    };

    ////////////////////////////////////////////////
    class IRunner;

    template<typename TRunner>
    class Runner;

    /// Base loop handler
    class IHandler
    {
    public:
        virtual ~IHandler() = default;
        virtual bool Started(IRunner& runner) = 0;
        virtual bool Update(IRunner& runner, const UpdateCtx& ctx) = 0;
        virtual void Stopping(IRunner& runner) = 0;
    };

    /// Base runner that runs the update action while the handler returns true
    class IRunner : public std::enable_shared_from_this<IRunner>
    {
    public:
        virtual ~IRunner() = default;

        using HandlerPtr = std::shared_ptr<IHandler>;
        virtual void Start(HandlerPtr handler) = 0;
        virtual void Finish(const FinishData& finishData) = 0;
    };

    /// Back-compatibility handler TODO: rm
    class FuncHandler : public IHandler
    {
    public:
        using UpdateAction = std::function<bool(const UpdateCtx&)>;
        explicit FuncHandler(UpdateAction&& updateAction) : _updateAction{std::move(updateAction)} {}

        bool Started(IRunner& runner) override { return true;}
        bool Update(IRunner& runner, const UpdateCtx& ctx) override { return _updateAction(ctx); }
        void Stopping(IRunner& runner) override {}

    private:
        UpdateAction _updateAction;
    };

    // template<typename TRunner>
    // class Handler : public IHandler
    // {
    // public:
    //     virtual bool Update(Runner<TRunner>& runner, const UpdateCtx& ctx) = 0;
    //
    // protected:
    //     bool Update(IRunner& runner, const UpdateCtx& ctx) override
    //     {
    //         static_assert(std::is_base_of_v<Runner<TRunner>, TRunner>);
    //         return static_cast<Runner<TRunner>&>(runner).Update(ctx);
    //     }
    // };
    //
    // template<typename TRunner>
    // class Runner : public IRunner
    // {
    // public:
    //     virtual void Start(UpdateAction updateAction) = 0;
    //
    // };
    //
    // struct MyRunner : Runner<MyRunner>
    // {
    //
    // };
}
