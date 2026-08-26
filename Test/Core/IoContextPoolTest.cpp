#include "Core/IoContextPool.hpp"
#include "gtest/gtest.h"

#include <chrono>
#include <future>
#include <stdexcept>
#include <type_traits>

#include "boost/asio/post.hpp"

namespace Flux::Test
{
    using namespace std::chrono_literals;

    constexpr auto FutureTimeout = 2s;

    static_assert(!std::is_copy_constructible_v<IoContextPool>);
    static_assert(!std::is_copy_assignable_v<IoContextPool>);
    static_assert(!std::is_move_constructible_v<IoContextPool>);
    static_assert(!std::is_move_assignable_v<IoContextPool>);

    TEST(IoContextPoolConstructorTest, DefaultConstructorExposesReadyContextAndStrand)
    {
        const IoContextPool pool;

        const auto* const ioContext = &pool.GetIoContext();
        const auto* const strand = &pool.GetStrand();

        EXPECT_FALSE(ioContext->stopped());
        EXPECT_FALSE(strand->running_in_this_thread());
        EXPECT_EQ(ioContext, &pool.GetIoContext());
        EXPECT_EQ(strand, &pool.GetStrand());
    }

    TEST(IoContextPoolConstructorTest, ZeroThreadCountFallsBackToUsablePool)
    {
        IoContextPool pool(0);
        std::promise<bool> handledOnStrandPromise;
        auto handledOnStrand = handledOnStrandPromise.get_future();

        boost::asio::post(pool.GetStrand(), [&pool, &handledOnStrandPromise] -> void {
            handledOnStrandPromise.set_value(pool.GetStrand().running_in_this_thread());
        });

        pool.Stop();
        pool.Run();
        pool.Run();

        const auto handlerStatus = handledOnStrand.wait_for(FutureTimeout);

        pool.Stop();
        pool.Stop();
        pool.Run();

        ASSERT_EQ(handlerStatus, std::future_status::ready);
        EXPECT_TRUE(handledOnStrand.get());
        EXPECT_TRUE(pool.GetIoContext().stopped());
    }

    TEST(IoContextPoolRunTest, ContinuesAfterStandardExceptionFromHandler)
    {
        IoContextPool pool(1);
        std::promise<void> continuationCalledPromise;
        const auto continuationCalled = continuationCalledPromise.get_future();

        boost::asio::post(pool.GetStrand(),
                          [] -> void { throw std::runtime_error("expected handler failure"); });
        boost::asio::post(pool.GetStrand(), [&continuationCalledPromise] -> void {
            continuationCalledPromise.set_value();
        });

        pool.Run();
        const auto handlerStatus = continuationCalled.wait_for(FutureTimeout);
        pool.Stop();

        EXPECT_EQ(handlerStatus, std::future_status::ready);
    }

    TEST(IoContextPoolStopTest, DrainsQueuedWorkBeforeJoiningThreads)
    {
        IoContextPool pool(1);
        std::promise<void> handlerCalledPromise;
        const auto handlerCalled = handlerCalledPromise.get_future();

        boost::asio::post(pool.GetStrand(),
                          [&handlerCalledPromise] -> void { handlerCalledPromise.set_value(); });

        pool.Run();
        pool.Stop();

        EXPECT_EQ(handlerCalled.wait_for(FutureTimeout), std::future_status::ready);
        EXPECT_TRUE(pool.GetIoContext().stopped());
    }

    TEST(IoContextPoolDestructorTest, HardStopsRunningContext)
    {
        {
            IoContextPool pool(1);
            pool.Run();

            EXPECT_FALSE(pool.GetIoContext().stopped());
        }

        SUCCEED();
    }

}   // namespace Flux::Test
