#pragma once
#include <memory>
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
        virtual bool Started(IRunner& runner) { return true; }
        virtual void Stopping(IRunner& runner) {}
        virtual bool Update(IRunner& runner, const UpdateCtx& ctx) = 0;
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

    /// Typed handler for specific runner type
    template <typename TRunner>
    class Handler : public IHandler
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

    /// Typed runner for specific runner type
    template <typename THandler>
    class Runner : public IRunner
    {
    public:
        using HandlerPtr = std::shared_ptr<THandler>;
        virtual void Start(HandlerPtr handler) = 0;

    private:
        void Start(IRunner::HandlerPtr handler) override
        { 
            auto specific = std::dynamic_pointer_cast<THandler>(std::move(handler));
            if (specific) {
                this->Start(std::move(specific));
            } else {
                struct Wrap : THandler {
                    IRunner::HandlerPtr _handler;
                    enum Tag { Dummy };
                    explicit Wrap(Tag tag, IRunner::HandlerPtr handler)
                        : _handler(std::move(handler))
                    {}
                    bool Started(typename THandler::Runner& runner) override { return _handler->Started(runner); }
                    void Stopping(typename THandler::Runner& runner) override { _handler->Stopping(runner); }
                    bool Update(typename THandler::Runner& runner, const UpdateCtx& ctx) override { return _handler->Update(runner, ctx); }
                };
                this->Start(std::static_pointer_cast<THandler>(std::make_shared<Wrap>(Wrap::Tag::Dummy, std::move(handler))));
            }
        }
    };
}
