#include "Exec/Context/PureLoopContext.h"
#include "ExecTestReceivers.h"

#include <gtest/gtest.h>
#include <stdexec/execution.hpp>

namespace {

using namespace Exec;
using namespace ExecTest;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

/// Minimal OperationBase node that sets a flag when executed.
/// Used to test the public Enqueue(OperationBase*) path without going through
/// the sender/receiver machinery.
struct ManualNode : OperationBase
{
    bool& flag;

    static void Run(OperationBase* base) noexcept
    {
        static_cast<ManualNode*>(base)->flag = true;
    }

    explicit ManualNode(bool& f) : flag(f) { this->execute = Run; }
};

} // namespace

// ---------------------------------------------------------------------------
// Empty queue
// ---------------------------------------------------------------------------

TEST(PureLoopContextTest, EmptyDrainReturnsZero)
{
    PureLoopContext ctx;
    EXPECT_EQ(ctx.DrainQueue(), 0u);
}

// ---------------------------------------------------------------------------
// Basic enqueue / drain behaviour
// ---------------------------------------------------------------------------

// start() must enqueue but must NOT execute — execution is deferred to DrainQueue().
TEST(PureLoopContextTest, NoExecutionBeforeDrain)
{
    PureLoopContext ctx;
    Outcome outcome;

    auto op = stdexec::connect(stdexec::schedule(ctx.GetScheduler()),
                               BasicReceiver{&outcome});
    stdexec::start(op);

    EXPECT_FALSE(outcome.valued);
    EXPECT_FALSE(outcome.stopped);
}

TEST(PureLoopContextTest, DrainExecutesSingleTask)
{
    PureLoopContext ctx;
    Outcome outcome;

    auto op = stdexec::connect(stdexec::schedule(ctx.GetScheduler()),
                               BasicReceiver{&outcome});
    stdexec::start(op);
    ctx.DrainQueue();

    EXPECT_TRUE(outcome.valued);
    EXPECT_FALSE(outcome.stopped);
}

TEST(PureLoopContextTest, DrainReturnsTaskCount)
{
    PureLoopContext ctx;
    Outcome a, b, c;

    auto opA = stdexec::connect(stdexec::schedule(ctx.GetScheduler()), BasicReceiver{&a});
    auto opB = stdexec::connect(stdexec::schedule(ctx.GetScheduler()), BasicReceiver{&b});
    auto opC = stdexec::connect(stdexec::schedule(ctx.GetScheduler()), BasicReceiver{&c});
    stdexec::start(opA);
    stdexec::start(opB);
    stdexec::start(opC);

    EXPECT_EQ(ctx.DrainQueue(), 3u);
    EXPECT_TRUE(a.valued);
    EXPECT_TRUE(b.valued);
    EXPECT_TRUE(c.valued);
}

// Draining an already-empty queue after all tasks have run must return 0.
TEST(PureLoopContextTest, SecondDrainOnEmptyQueueReturnsZero)
{
    PureLoopContext ctx;
    Outcome outcome;

    auto op = stdexec::connect(stdexec::schedule(ctx.GetScheduler()),
                               BasicReceiver{&outcome});
    stdexec::start(op);
    ctx.DrainQueue();

    EXPECT_EQ(ctx.DrainQueue(), 0u);
}

// Each drain call only processes tasks enqueued before/during that call.
TEST(PureLoopContextTest, MultipleIndependentDrains)
{
    PureLoopContext ctx;
    Outcome first, second;

    auto op1 = stdexec::connect(stdexec::schedule(ctx.GetScheduler()),
                                BasicReceiver{&first});
    stdexec::start(op1);
    EXPECT_EQ(ctx.DrainQueue(), 1u);
    EXPECT_TRUE(first.valued);

    auto op2 = stdexec::connect(stdexec::schedule(ctx.GetScheduler()),
                                BasicReceiver{&second});
    stdexec::start(op2);
    EXPECT_EQ(ctx.DrainQueue(), 1u);
    EXPECT_TRUE(second.valued);
}

// ---------------------------------------------------------------------------
// Stop-token behaviour
// ---------------------------------------------------------------------------

// Stop requested before start() → the task observes stop at drain time → set_stopped.
TEST(PureLoopContextTest, PreStoppedTokenYieldsSetStopped)
{
    PureLoopContext ctx;
    Outcome outcome;
    stdexec::inplace_stop_source source;
    source.request_stop();

    auto op = stdexec::connect(stdexec::schedule(ctx.GetScheduler()),
                               StopReceiver{.outcome=&outcome, .token=source.get_token()});
    stdexec::start(op);
    ctx.DrainQueue();

    EXPECT_FALSE(outcome.valued);
    EXPECT_TRUE(outcome.stopped);
}

// Stop requested after start() but before DrainQueue() → stop is still observed
// because stop_requested() is checked inside Execute(), not inside start().
TEST(PureLoopContextTest, StopAfterStartButBeforeDrainYieldsSetStopped)
{
    PureLoopContext ctx;
    Outcome outcome;
    stdexec::inplace_stop_source source;

    auto op = stdexec::connect(stdexec::schedule(ctx.GetScheduler()),
                               StopReceiver{.outcome=&outcome, .token=source.get_token()});
    stdexec::start(op);
    source.request_stop(); // after enqueue, before drain
    ctx.DrainQueue();

    EXPECT_FALSE(outcome.valued);
    EXPECT_TRUE(outcome.stopped);
}

// No stop token provided → never_stop_token fallback → set_value always taken.
TEST(PureLoopContextTest, NoStopTokenYieldsSetValue)
{
    PureLoopContext ctx;
    Outcome outcome;

    auto op = stdexec::connect(stdexec::schedule(ctx.GetScheduler()),
                               BasicReceiver{&outcome});
    stdexec::start(op);
    ctx.DrainQueue();

    EXPECT_TRUE(outcome.valued);
    EXPECT_FALSE(outcome.stopped);
}

// Mixed tasks: some stopped, some not.
TEST(PureLoopContextTest, MixedStopTokensInSameDrain)
{
    PureLoopContext ctx;
    Outcome running, cancelled;
    stdexec::inplace_stop_source source;
    source.request_stop();

    auto opRun = stdexec::connect(stdexec::schedule(ctx.GetScheduler()),
                                  BasicReceiver{&running});
    auto opCan = stdexec::connect(stdexec::schedule(ctx.GetScheduler()),
                                  StopReceiver{.outcome=&cancelled, .token=source.get_token()});
    stdexec::start(opRun);
    stdexec::start(opCan);
    EXPECT_EQ(ctx.DrainQueue(), 2u);

    EXPECT_TRUE(running.valued);
    EXPECT_FALSE(running.stopped);
    EXPECT_FALSE(cancelled.valued);
    EXPECT_TRUE(cancelled.stopped);
}

// ---------------------------------------------------------------------------
// External Enqueue (public API for OperationBase* nodes from outside)
// ---------------------------------------------------------------------------

TEST(PureLoopContextTest, ExternalEnqueueExecutedByDrain)
{
    PureLoopContext ctx;
    bool executed = false;

    ManualNode node{executed};
    ctx.Enqueue(&node);

    EXPECT_FALSE(executed);
    EXPECT_EQ(ctx.DrainQueue(), 1u);
    EXPECT_TRUE(executed);
}

// Multiple external nodes are all processed in a single DrainQueue().
TEST(PureLoopContextTest, MultipleExternalEnqueuesDrainedTogether)
{
    PureLoopContext ctx;
    bool a = false, b = false, c = false;

    ManualNode na{a}, nb{b}, nc{c};
    ctx.Enqueue(&na);
    ctx.Enqueue(&nb);
    ctx.Enqueue(&nc);

    EXPECT_EQ(ctx.DrainQueue(), 3u);
    EXPECT_TRUE(a);
    EXPECT_TRUE(b);
    EXPECT_TRUE(c);
}

// ---------------------------------------------------------------------------
// Greedy drain
// ---------------------------------------------------------------------------

// A node that enqueues a second node during its own execution.
// DrainQueue() must process both in a single call (greedy / exhaustive drain).
TEST(PureLoopContextTest, DrainIsGreedy)
{
    PureLoopContext ctx;
    bool first = false, second = false;

    ManualNode secondNode{second};

    struct ChainNode : OperationBase
    {
        PureLoopContext* ctx;
        bool& flag;
        OperationBase* next;

        static void Run(OperationBase* base) noexcept
        {
            auto& self = *static_cast<ChainNode*>(base);
            self.flag = true;
            self.ctx->Enqueue(self.next);
        }

        ChainNode(PureLoopContext* c, bool& f, OperationBase* n)
            : ctx(c), flag(f), next(n)
        {
            this->execute = Run;
        }
    };

    ChainNode firstNode{&ctx, first, &secondNode};
    ctx.Enqueue(&firstNode);

    const std::size_t count = ctx.DrainQueue();

    EXPECT_EQ(count, 2u);
    EXPECT_TRUE(first);
    EXPECT_TRUE(second);
}

// ---------------------------------------------------------------------------
// Scheduler equality
// ---------------------------------------------------------------------------

TEST(PureLoopContextTest, SchedulersFromSameContextAreEqual)
{
    PureLoopContext ctx;
    EXPECT_EQ(ctx.GetScheduler(), ctx.GetScheduler());
}

TEST(PureLoopContextTest, SchedulersFromDifferentContextsAreNotEqual)
{
    PureLoopContext ctx1;
    PureLoopContext ctx2;
    EXPECT_NE(ctx1.GetScheduler(), ctx2.GetScheduler());
}
