#include "Core/IoContextPool.hpp"

#include <chrono>
#include <cstddef>
#include <future>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include "boost/asio/post.hpp"

#include "gtest/gtest.h"

namespace Flux::Test
{
    using namespace std::chrono_literals;

    constexpr auto FutureTimeout = 2s;
    constexpr std::size_t NoThreadCount = 0U;
    constexpr std::size_t SingleThreadCount = 1U;
    constexpr std::string_view RunStatusError =
        "IoContextPool::Run(): IO context pool status is not Status::Started";
    constexpr std::string_view StopStatusError =
        "IoContextPool::Stop():IO context pool status is not Status::Running";
    constexpr std::string_view StopFromWorkerThreadError =
        "IoContextPool::Stop(): cannot be called from an IO worker thread";

    static_assert(!std::is_copy_constructible_v<IoContextPool>);
    static_assert(!std::is_copy_assignable_v<IoContextPool>);
    static_assert(!std::is_move_constructible_v<IoContextPool>);
    static_assert(!std::is_move_assignable_v<IoContextPool>);
    static_assert(std::is_destructible_v<IoContextPool>);

    TEST(IoContextPoolConstructorTest, DefaultConstructorExposesReadyContextAndStableStrand)
    {
        const IoContextPool pool;

        const auto* const ioContext = &pool.GetIoContext();
        const auto* const strand = &pool.GetStrand();

        EXPECT_FALSE(ioContext->stopped());
        EXPECT_FALSE(strand->running_in_this_thread());
        EXPECT_EQ(ioContext, &pool.GetIoContext());
        EXPECT_EQ(strand, &pool.GetStrand());
    }

    TEST(IoContextPoolConstructorTest, ZeroThreadCountFallsBackToAUsableWorker)
    {
        IoContextPool pool(NoThreadCount);
        std::promise<bool> handledOnStrandPromise;
        auto handledOnStrand = handledOnStrandPromise.get_future();

        boost::asio::post(pool.GetStrand(), [&pool, &handledOnStrandPromise] -> void {
            handledOnStrandPromise.set_value(pool.GetStrand().running_in_this_thread());
        });

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.value_or(false));

        const auto handlerStatus = handledOnStrand.wait_for(FutureTimeout);
        const auto stopResult = pool.Stop();

        EXPECT_TRUE(stopResult.value_or(false));
        ASSERT_EQ(handlerStatus, std::future_status::ready);
        EXPECT_TRUE(handledOnStrand.get());
        EXPECT_TRUE(pool.GetIoContext().stopped());
    }

    TEST(IoContextPoolRunTest, StartsWorkerThreadsAndExecutesPostedWork)
    {
        IoContextPool pool(SingleThreadCount);
        std::promise<bool> handledOnStrandPromise;
        auto handledOnStrand = handledOnStrandPromise.get_future();

        boost::asio::post(pool.GetStrand(), [&pool, &handledOnStrandPromise] -> void {
            handledOnStrandPromise.set_value(pool.GetStrand().running_in_this_thread());
        });

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.value_or(false));

        const auto handlerStatus = handledOnStrand.wait_for(FutureTimeout);
        const auto stopResult = pool.Stop();

        EXPECT_TRUE(stopResult.value_or(false));
        ASSERT_EQ(handlerStatus, std::future_status::ready);
        EXPECT_TRUE(handledOnStrand.get());
    }

    TEST(IoContextPoolRunTest, RejectsRepeatedRunWhileAlreadyRunning)
    {
        IoContextPool pool(SingleThreadCount);

        const auto firstRunResult = pool.Run();
        EXPECT_TRUE(firstRunResult.value_or(false));

        const auto secondRunResult = pool.Run();
        const auto stopResult = pool.Stop();

        EXPECT_TRUE(stopResult.value_or(false));
        ASSERT_FALSE(secondRunResult.has_value());
        EXPECT_EQ(secondRunResult.error(), RunStatusError);
    }

    TEST(IoContextPoolRunTest, RejectsRunAfterStop)
    {
        IoContextPool pool(SingleThreadCount);

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.value_or(false));

        const auto stopResult = pool.Stop();
        const auto secondRunResult = pool.Run();

        EXPECT_TRUE(stopResult.value_or(false));
        ASSERT_FALSE(secondRunResult.has_value());
        EXPECT_EQ(secondRunResult.error(), RunStatusError);
    }

    TEST(IoContextPoolRunTest, ContinuesAfterStandardExceptionFromHandler)
    {
        IoContextPool pool(SingleThreadCount);
        std::promise<void> continuationCalledPromise;
        auto continuationCalled = continuationCalledPromise.get_future();

        boost::asio::post(pool.GetStrand(),
                          [] -> void { throw std::runtime_error("expected handler failure"); });
        boost::asio::post(pool.GetStrand(), [&continuationCalledPromise] -> void {
            continuationCalledPromise.set_value();
        });

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.value_or(false));

        const auto handlerStatus = continuationCalled.wait_for(FutureTimeout);
        const auto stopResult = pool.Stop();

        EXPECT_TRUE(stopResult.value_or(false));
        EXPECT_EQ(handlerStatus, std::future_status::ready);
    }

    TEST(IoContextPoolStopTest, RejectsStopBeforeRun)
    {
        IoContextPool pool(SingleThreadCount);

        const auto stopResult = pool.Stop();

        ASSERT_FALSE(stopResult.has_value());
        EXPECT_EQ(stopResult.error(), StopStatusError);
    }

    TEST(IoContextPoolStopTest, DrainsQueuedWorkBeforeJoiningThreads)
    {
        IoContextPool pool(SingleThreadCount);
        std::promise<void> handlerCalledPromise;
        auto handlerCalled = handlerCalledPromise.get_future();

        boost::asio::post(pool.GetStrand(),
                          [&handlerCalledPromise] -> void { handlerCalledPromise.set_value(); });

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.value_or(false));

        const auto stopResult = pool.Stop();
        const auto handlerStatus = handlerCalled.wait_for(FutureTimeout);

        EXPECT_TRUE(stopResult.value_or(false));
        EXPECT_EQ(handlerStatus, std::future_status::ready);
        EXPECT_TRUE(pool.GetIoContext().stopped());
    }

    TEST(IoContextPoolStopTest, RejectsRepeatedStopAfterSuccessfulStop)
    {
        IoContextPool pool(SingleThreadCount);

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.value_or(false));

        const auto firstStopResult = pool.Stop();
        const auto secondStopResult = pool.Stop();

        EXPECT_TRUE(firstStopResult.value_or(false));
        ASSERT_FALSE(secondStopResult.has_value());
        EXPECT_EQ(secondStopResult.error(), StopStatusError);
    }

    TEST(IoContextPoolStopTest, RejectsStopFromWorkerThread)
    {
        IoContextPool pool(SingleThreadCount);
        std::promise<std::string> workerStopResultPromise;
        auto workerStopResult = workerStopResultPromise.get_future();

        boost::asio::post(pool.GetStrand(), [&pool, &workerStopResultPromise] -> void {
            const auto stopResult = pool.Stop();
            workerStopResultPromise.set_value(stopResult.has_value() ? std::string{}
                                                                    : stopResult.error());
        });

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.value_or(false));

        const auto workerStopStatus = workerStopResult.wait_for(FutureTimeout);
        const auto stopResult = pool.Stop();

        EXPECT_TRUE(stopResult.value_or(false));
        ASSERT_EQ(workerStopStatus, std::future_status::ready);
        EXPECT_EQ(workerStopResult.get(), StopFromWorkerThreadError);
    }

    TEST(IoContextPoolDestructorTest, DestroysCleanlyAfterExplicitStop)
    {
        {
            IoContextPool pool(SingleThreadCount);

            const auto runResult = pool.Run();
            EXPECT_TRUE(runResult.value_or(false));

            const auto stopResult = pool.Stop();
            EXPECT_TRUE(stopResult.value_or(false));
        }

        SUCCEED();
    }

    TEST(IoContextPoolDestructorTest, HardStopsRunningContext)
    {
        {
            IoContextPool pool(SingleThreadCount);

            const auto runResult = pool.Run();
            ASSERT_TRUE(runResult.value_or(false));

            EXPECT_FALSE(pool.GetIoContext().stopped());
        }

        SUCCEED();
    }

}   // namespace Flux::Test
