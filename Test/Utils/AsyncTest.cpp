#include "Utils/Async.hpp"

#include "gtest/gtest.h"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <cstddef>
#include <future>
#include <stdexcept>
#include <thread>

namespace Flux::Test
{
    using namespace std::chrono_literals;

    TEST(AsyncTest, DispatchOperationCompletesSuccessfully)
    {
        boost::asio::io_context context;
        const auto work = boost::asio::make_work_guard(context);
        const boost::asio::strand<boost::asio::io_context::executor_type> strand(
            context.get_executor());
        const boost::asio::any_io_executor executor = strand;

        auto future = DispatchOperation(executor, [] -> Result { return Result {}; });

        std::thread worker([&context] -> void { context.run(); });

        ASSERT_EQ(future.wait_for(3s), std::future_status::ready);
        const auto result = future.get();
        ASSERT_TRUE(result.has_value());

        context.stop();
        worker.join();
    }

    TEST(AsyncTest, DispatchOperationReturnsError)
    {
        boost::asio::io_context context;
        const auto work = boost::asio::make_work_guard(context);
        const boost::asio::strand<boost::asio::io_context::executor_type> strand(
            context.get_executor());
        const boost::asio::any_io_executor executor = strand;

        auto future = DispatchOperation(executor, [] -> Result {
            return std::unexpected(MakeErrorCode(Error::InvalidArgument));
        });

        std::thread worker([&context] -> void { context.run(); });

        ASSERT_EQ(future.wait_for(3s), std::future_status::ready);
        const auto result = future.get();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), MakeErrorCode(Error::InvalidArgument));

        context.stop();
        worker.join();
    }

    TEST(AsyncTest, DispatchOperationCapturesLambdaState)
    {
        boost::asio::io_context context;
        const auto work = boost::asio::make_work_guard(context);
        const boost::asio::strand<boost::asio::io_context::executor_type> strand(
            context.get_executor());
        const boost::asio::any_io_executor executor = strand;

        constexpr int value = 42;
        auto future = DispatchOperation(executor, [&value] -> Result {
            EXPECT_EQ(value, 42);
            return Result {};
        });

        std::thread worker([&context] -> void { context.run(); });

        ASSERT_EQ(future.wait_for(3s), std::future_status::ready);
        const auto result = future.get();
        ASSERT_TRUE(result.has_value());

        context.stop();
        worker.join();
    }

    TEST(AsyncTest, DispatchOperationMultipleSequential)
    {
        boost::asio::io_context context;
        const auto work = boost::asio::make_work_guard(context);
        const boost::asio::strand<boost::asio::io_context::executor_type> strand(
            context.get_executor());
        const boost::asio::any_io_executor executor = strand;

        std::vector<std::future<Result>> futures;
        constexpr auto value = 10;
        for (std::size_t i = 0; i < value; ++i)
        {
            futures.push_back(DispatchOperation(executor, [] -> Result { return Result {}; }));
        }

        std::thread worker([&context] -> void { context.run(); });

        for (auto& future : futures)
        {
            ASSERT_EQ(future.wait_for(3s), std::future_status::ready);
            const auto result = future.get();
            ASSERT_TRUE(result.has_value());
        }

        context.stop();
        worker.join();
    }
}   // namespace Flux::Test
