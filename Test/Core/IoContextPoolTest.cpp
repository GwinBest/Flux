#include "Core/IoContextPool.hpp"
#include "gtest/gtest.h"

#include <chrono>
#include <cstddef>
#include <exception>
#include <future>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "boost/asio/post.hpp"

namespace Flux::Test
{
    namespace
    {
        using namespace std::chrono_literals;

        constexpr auto FutureTimeout = 2s;
        constexpr std::size_t NoThreadCount = 0U;
        constexpr std::size_t SingleThreadCount = 1U;

        template<typename TValue>
        auto ErrorMessage(const std::expected<TValue, ErrorCode>& result) -> std::string
        {
            return result.has_value() ? std::string {} : result.error().message();
        }
    }   // namespace

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
        EXPECT_TRUE(runResult.has_value()) << ErrorMessage(runResult);

        const auto handlerStatus = handledOnStrand.wait_for(FutureTimeout);
        const auto stopResult = pool.Stop();

        EXPECT_TRUE(stopResult.has_value()) << ErrorMessage(stopResult);
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
        EXPECT_TRUE(runResult.has_value()) << ErrorMessage(runResult);

        const auto handlerStatus = handledOnStrand.wait_for(FutureTimeout);
        const auto stopResult = pool.Stop();

        EXPECT_TRUE(stopResult.has_value()) << ErrorMessage(stopResult);
        ASSERT_EQ(handlerStatus, std::future_status::ready);
        EXPECT_TRUE(handledOnStrand.get());
    }

    TEST(IoContextPoolRunTest, RejectsRepeatedRunWhileAlreadyRunning)
    {
        IoContextPool pool(SingleThreadCount);

        const auto firstRunResult = pool.Run();
        EXPECT_TRUE(firstRunResult.has_value()) << ErrorMessage(firstRunResult);

        const auto secondRunResult = pool.Run();
        const auto stopResult = pool.Stop();

        EXPECT_TRUE(stopResult.has_value()) << ErrorMessage(stopResult);
        ASSERT_FALSE(secondRunResult.has_value());
        EXPECT_EQ(secondRunResult.error(), MakeErrorCode(Error::IoContextPoolNotStarted));
    }

    TEST(IoContextPoolRunTest, RejectsRunAfterStop)
    {
        IoContextPool pool(SingleThreadCount);

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.has_value()) << ErrorMessage(runResult);

        const auto stopResult = pool.Stop();
        const auto secondRunResult = pool.Run();

        EXPECT_TRUE(stopResult.has_value()) << ErrorMessage(stopResult);
        ASSERT_FALSE(secondRunResult.has_value());
        EXPECT_EQ(secondRunResult.error(), MakeErrorCode(Error::IoContextPoolNotStarted));
    }

    TEST(IoContextPoolRunTest, ContinuesAfterStandardExceptionFromHandler)
    {
        IoContextPool pool(SingleThreadCount);
        std::promise<void> continuationCalledPromise;
        const auto continuationCalled = continuationCalledPromise.get_future();

        boost::asio::post(pool.GetStrand(),
                          [] -> void { throw std::runtime_error("expected handler failure"); });
        boost::asio::post(pool.GetStrand(), [&continuationCalledPromise] -> void {
            continuationCalledPromise.set_value();
        });

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.has_value()) << ErrorMessage(runResult);

        const auto handlerStatus = continuationCalled.wait_for(FutureTimeout);
        const auto stopResult = pool.Stop();

        EXPECT_TRUE(stopResult.has_value()) << ErrorMessage(stopResult);
        EXPECT_EQ(handlerStatus, std::future_status::ready);
    }

    TEST(IoContextPoolRunTest, ContinuesAfterNonStandardExceptionFromHandler)
    {
        struct NonStandardFailure
        {};

        IoContextPool pool(SingleThreadCount);
        std::promise<void> continued;
        const auto continuation = continued.get_future();
        const auto failure = std::make_exception_ptr(NonStandardFailure {});
        boost::asio::post(pool.GetStrand(), [failure] -> void { std::rethrow_exception(failure); });
        boost::asio::post(pool.GetStrand(), [&continued] -> void { continued.set_value(); });

        ASSERT_TRUE(pool.Run().has_value());
        const auto status = continuation.wait_for(FutureTimeout);
        EXPECT_TRUE(pool.Stop().has_value());
        EXPECT_EQ(status, std::future_status::ready);
    }

    TEST(IoContextPoolRunTest, MultipleWorkersDrainAllStrandHandlersInOrder)
    {
        IoContextPool pool(4);
        std::vector<std::size_t> completed;
        std::vector<std::size_t> expected;
        bool allOnStrand = true;
        constexpr std::size_t handlerCount = 100;
        for (std::size_t index = 0; index < handlerCount; ++index)
        {
            expected.push_back(index);
            boost::asio::post(pool.GetStrand(), [&pool, &completed, &allOnStrand, index] -> void {
                allOnStrand = allOnStrand && pool.GetStrand().running_in_this_thread();
                completed.push_back(index);
            });
        }

        ASSERT_TRUE(pool.Run().has_value());
        EXPECT_TRUE(pool.Stop().has_value());
        EXPECT_TRUE(allOnStrand);
        EXPECT_EQ(completed, expected);
    }

    TEST(IoContextPoolStopTest, RejectsStopBeforeRun)
    {
        IoContextPool pool(SingleThreadCount);

        const auto stopResult = pool.Stop();

        ASSERT_FALSE(stopResult.has_value());
        EXPECT_EQ(stopResult.error(), MakeErrorCode(Error::IoContextPoolNotRunning));
    }

    TEST(IoContextPoolStopTest, DrainsQueuedWorkBeforeJoiningThreads)
    {
        IoContextPool pool(SingleThreadCount);
        std::promise<void> handlerCalledPromise;
        const auto handlerCalled = handlerCalledPromise.get_future();

        boost::asio::post(pool.GetStrand(),
                          [&handlerCalledPromise] -> void { handlerCalledPromise.set_value(); });

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.has_value()) << ErrorMessage(runResult);

        const auto stopResult = pool.Stop();
        const auto handlerStatus = handlerCalled.wait_for(FutureTimeout);

        EXPECT_TRUE(stopResult.has_value()) << ErrorMessage(stopResult);
        EXPECT_EQ(handlerStatus, std::future_status::ready);
        EXPECT_TRUE(pool.GetIoContext().stopped());
    }

    TEST(IoContextPoolStopTest, RejectsRepeatedStopAfterSuccessfulStop)
    {
        IoContextPool pool(SingleThreadCount);

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.has_value()) << ErrorMessage(runResult);

        const auto firstStopResult = pool.Stop();
        const auto secondStopResult = pool.Stop();

        EXPECT_TRUE(firstStopResult.has_value()) << ErrorMessage(firstStopResult);
        ASSERT_FALSE(secondStopResult.has_value());
        EXPECT_EQ(secondStopResult.error(), MakeErrorCode(Error::IoContextPoolNotRunning));
    }

    TEST(IoContextPoolStopTest, RejectsStopFromWorkerThread)
    {
        IoContextPool pool(SingleThreadCount);
        std::promise<ErrorCode> workerStopResultPromise;
        auto workerStopResult = workerStopResultPromise.get_future();

        boost::asio::post(pool.GetStrand(), [&pool, &workerStopResultPromise] -> void {
            const auto stopResult = pool.Stop();
            workerStopResultPromise.set_value(stopResult.has_value() ? ErrorCode {}
                                                                     : stopResult.error());
        });

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.has_value()) << ErrorMessage(runResult);

        const auto workerStopStatus = workerStopResult.wait_for(FutureTimeout);
        const auto stopResult = pool.Stop();

        EXPECT_TRUE(stopResult.has_value()) << ErrorMessage(stopResult);
        ASSERT_EQ(workerStopStatus, std::future_status::ready);
        EXPECT_EQ(workerStopResult.get(), MakeErrorCode(Error::IoContextPoolStopFromWorkerThread));
    }

    TEST(IoContextPoolDestructorTest, DestroysCleanlyAfterExplicitStop)
    {
        {
            IoContextPool pool(SingleThreadCount);

            const auto runResult = pool.Run();
            EXPECT_TRUE(runResult.has_value()) << ErrorMessage(runResult);

            const auto stopResult = pool.Stop();
            EXPECT_TRUE(stopResult.has_value()) << ErrorMessage(stopResult);
        }

        SUCCEED();
    }

    TEST(IoContextPoolDestructorTest, HardStopsRunningContext)
    {
        {
            IoContextPool pool(SingleThreadCount);

            const auto runResult = pool.Run();
            ASSERT_TRUE(runResult.has_value()) << ErrorMessage(runResult);

            EXPECT_FALSE(pool.GetIoContext().stopped());
        }

        SUCCEED();
    }
}   // namespace Flux::Test
