#include "Log/Log.h"
#include "Boot/Boot.h"

#include <algorithm>
#include <boost/intrusive/list.hpp>
#include <cstdlib>
#include <memory>
#include <cassert>

#include "App/Loop/TightRunner.h"

using IRunner = App::Loop::IRunner;
using UpdateCtx = App::Loop::UpdateCtx;
using TightRunner = App::Loop::TightRunner;

/// Action interface
template <class T>
class IAction;

template <class TRet, class... TArgs>
class IAction<TRet(TArgs...)>
{
public:
    virtual ~IAction() = default;
    virtual TRet operator()(TArgs... args) = 0;
};

/// Trait to extract the signature from invocable types
template <typename T>
struct function_signature;

template <typename T, typename TRet, typename... TArgs>
struct function_signature<TRet (T::*)(TArgs...) const>
{
    using type = TRet(TArgs...);
};

template <typename T, typename TRet, typename... TArgs>
struct function_signature<TRet (T::*)(TArgs...)>
{
    using type = TRet(TArgs...);
};

/// Action implementation wrapping a callable
template <typename T, typename Signature>
class Action;

template <typename T, typename TRet, typename... TArgs>
requires std::is_invocable_r_v<TRet, T, TArgs...>
class Action<T, TRet(TArgs...)> : public IAction<TRet(TArgs...)>
{
public:
    explicit Action(T impl)
        : _impl(std::move(impl))
    {}

    TRet operator()(TArgs... args) override { return _impl(std::forward<TArgs>(args)...); }

private:
    T _impl;
};

template <typename T>
Action(T) -> Action<T, typename function_signature<decltype(&T::operator())>::type>;

/// Composite action implementation
template <typename TRet, typename... TArgs>
class CompositeAction : public IAction<TRet(TArgs...)>
{
public:
    void AddAction(std::shared_ptr<IAction<TRet(TArgs...)>> action)
    {
        _actions.push_back(std::move(action));
    }
    TRet operator()(TArgs... args) override
    {
        TRet result{};
        for (const auto& action : _actions) {
            result = action->operator()(std::forward<TArgs>(args)...);
        }
        return result;
    }
private:
    std::vector<std::shared_ptr<IAction<TRet(TArgs...)>>> _actions;
};

/// Example usage
using IStartAction = IAction<void(IRunner&)>;
using IUpdateAction = IAction<void(IRunner&, const UpdateCtx&)>;
struct MyAction
    : public IStartAction
    , public IUpdateAction
{
    void operator()(IRunner& runner) override
    {
        Log::Info("MyAction executed");
    }
    void operator()(IRunner& runner, const UpdateCtx&) override
    {
        Log::Info("MyAction executed");
    }
};

void Foo() 
{
    MyAction myAction;
    //using StartAction = Action<void(IRunner&)>;

    auto startAction1 = Action([](IRunner& runner) {
        Log::Info("Action1");
    });
    auto startAction2 = Action([](IRunner& runner) {
        Log::Info("Action2");
    });

    // auto compositeAction = CompositeAction<void, IRunner&>();
    // compositeAction.AddAction(std::make_shared<IStartAction>(std::move(startAction1)));
    // compositeAction.AddAction(std::make_shared<IStartAction>(std::move(startAction2)));

    IStartAction* actionPtr = &startAction1;
    TightRunner runner;
    actionPtr->operator()(runner);
}

// //////////////////////////////////////////////////////
// struct CompositeUpdater;
// struct IUpdater : public boost::intrusive::list_base_hook<>
// {
//     using List = boost::intrusive::list<struct IUpdater>;
//     IUpdater() = default;
//     IUpdater(const IUpdater&) = delete;
//     IUpdater(IUpdater&&) = delete;
//     IUpdater& operator=(const IUpdater&) const = delete;
//     IUpdater& operator=(IUpdater&&) = delete;
//     virtual ~IUpdater() = default;
//     virtual bool Started() { return true; }
//     virtual void Stopped() {}
//     virtual void Update(const UpdateCtx& ctx) = 0;
//     void RemoveFrom();
// private:
//     friend struct CompositeUpdater;
//     CompositeUpdater* _parent{};
//     void InternalAdd(CompositeUpdater& parent) { _parent = &parent; }
//     void InternalRemove() { _parent = nullptr; }
// };

////////////////////////////////////////////////////////////////
struct BaseUpdater
{
    virtual ~BaseUpdater() = default;
    virtual void Start() = 0;
    virtual void Update(float elapsed) = 0;
    virtual void Complete() = 0;
};

template <typename TUpdater>
struct SingleRunner: BaseUpdater
{
    SingleRunner(TUpdater inner)
        : _inner(std::move(inner))
    {}
    void Start() override { get().Start(); }
    void Update(float elapsed) override { get().Update(elapsed); }
    void Complete() override { get().Complete(); }

private:
    TUpdater _inner;
    decltype(auto) get()
    {
        if constexpr (requires { *_inner; }) {
            return *_inner;
        } else {
            return _inner;
        }
    }
};

struct Updater
    : BaseUpdater
    , boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>
{
    using List = boost::intrusive::list<Updater, boost::intrusive::constant_time_size<false>>;
};

struct MultiRunner: Updater
{
    void Add(Updater& item)
    {
        _items.push_back(item);
        if (_running) {
            item.Start();
        }
    }
    void Remove(Updater& item)
    {
        if (_running) {
            item.Complete();
        }
        _items.erase(_items.iterator_to(item));
    }

    void Start() override
    {
        _running = true;
        for (auto& item : _items) {
            item.Start();
        }
    }
    void Update(float elapsed) override
    {
        for (auto& item : _items) {
            item.Update(elapsed);
        }
    }
    void Complete() override
    {
        for (auto& item : _items) {
            item.Complete();
        }
        _running = false;
    }

private:
    // using PtrRunner::Update;
    Updater::List _items;
    bool _running{};
};

using PtrRunner = SingleRunner<std::shared_ptr<Updater>>;
struct SimpleRunner: PtrRunner
{
    using PtrRunner::PtrRunner;
    void Start() override
    {
        Log::Info("SimpleRunner Start");
        _running = true;
        PtrRunner::Start();
        for (auto i = 0; i < 3 && _running; ++i) {
            PtrRunner::Update(0.01f * static_cast<float>(i + 1));
        }
        PtrRunner::Complete();
        Log::Info("SimpleRunner Complete");
    }

    void Complete() override
    {
        Log::Info("SimpleRunner Complete Override");
        PtrRunner::Complete();
    }

private:
    //using PtrRunner::Update;
    bool _running{};
};

struct MyUpdater: Updater
{
    int _id;
    MyUpdater(int id): _id(id) { Log::Info("MyUpdater Created [{}]", _id); }
    ~MyUpdater() { Log::Info("MyUpdater Destroyed [{}]", _id); }
    void Start() override { Log::Info("MyUpdater Start [{}]", _id); }
    void Update(float elapsed) override { Log::Info("MyUpdater Update [{}]: {}", _id, elapsed); }
    void Complete() override { Log::Info("MyUpdater Complete [{}]", _id); }
};


int main(const int argc, const char** argv)
{
    Boot::LogHeader(argc, argv);
    Boot::SetupAbortHandlers();
    auto multiRunner = std::make_shared<MultiRunner>();
    auto osRunner = SimpleRunner{multiRunner};

    auto my1 = MyUpdater{1};
    multiRunner->Add(my1);
    osRunner.Start();
    {
        auto my2 = MyUpdater{2};
        multiRunner->Add(my2);
        osRunner.Update(0.1f);
    }
    return argc - 1; // to check exit code is passed from emrun
}
