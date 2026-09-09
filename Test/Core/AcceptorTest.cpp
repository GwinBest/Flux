#include "Core/Acceptor.hpp"
#include "gtest/gtest.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <expected>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "Core/IoContextPool.hpp"

namespace Flux::Test
{
    using namespace std::chrono_literals;

    namespace
    {
        using Tcp = boost::asio::ip::tcp;

        constexpr std::size_t SingleThreadCount = 1U;
        constexpr unsigned short AnyPort = 0U;

        auto Start(const std::shared_ptr<Acceptor>& acceptor) -> std::future<Result>
        {
            auto completion = std::make_shared<std::promise<Result>>();
            auto future = completion->get_future();
            acceptor->AsyncStartAccepting(
                [completion](Result result) -> void { completion->set_value(std::move(result)); });
            return future;
        }

        auto Stop(const std::shared_ptr<Acceptor>& acceptor) -> std::future<Result>
        {
            auto completion = std::make_shared<std::promise<Result>>();
            auto future = completion->get_future();
            acceptor->AsyncStopAccepting(
                [completion](Result result) -> void { completion->set_value(std::move(result)); });
            return future;
        }

        auto Await(std::future<Result> future) -> Result
        {
            if (future.wait_for(3s) != std::future_status::ready)
                throw std::runtime_error("Acceptor command did not complete");
            return future.get();
        }

        template<typename TValue>
        auto ErrorMessage(const std::expected<TValue, ErrorCode>& result) -> std::string
        {
            return result.has_value() ? std::string {} : result.error().message();
        }

        auto MakeLoopbackEndpoint() -> Tcp::endpoint
        {
            return {boost::asio::ip::address_v4::loopback(), AnyPort};
        }

        auto ReserveLoopbackEndpoint() -> Tcp::endpoint
        {
            boost::asio::io_context ioContext;
            Tcp::acceptor acceptor(ioContext);
            boost::system::error_code ignoredError;

            acceptor.open(Tcp::v4());
            acceptor.bind(MakeLoopbackEndpoint());

            const auto endpoint = acceptor.local_endpoint();
            ignoredError = acceptor.close(ignoredError);

            return endpoint;
        }
    }   // namespace

    static_assert(!std::is_copy_constructible_v<Acceptor>);
    static_assert(!std::is_copy_assignable_v<Acceptor>);
    static_assert(!std::is_move_constructible_v<Acceptor>);
    static_assert(!std::is_move_assignable_v<Acceptor>);
    static_assert(std::is_nothrow_destructible_v<Acceptor>);

    TEST(AcceptorFactoryTest, CreatesSharedAcceptorBoundToEndpoint)
    {
        IoContextPool pool(SingleThreadCount);
        ASSERT_TRUE(pool.Run().has_value());

        const auto acceptorResult = Acceptor::CreateAcceptor(pool.GetStrand(),
                                                             MakeLoopbackEndpoint());

        ASSERT_TRUE(acceptorResult.has_value()) << ErrorMessage(acceptorResult);
        const auto& acceptor = *acceptorResult;
        ASSERT_NE(acceptor, nullptr);
        const auto stopResult = Await(Stop(acceptor));
        EXPECT_TRUE(stopResult.has_value()) << ErrorMessage(stopResult);
    }

    TEST(AcceptorStartAcceptingTest, StartsAcceptingOnceFromInitializedState)
    {
        IoContextPool pool(SingleThreadCount);
        ASSERT_TRUE(pool.Run().has_value());
        const auto acceptorResult = Acceptor::CreateAcceptor(pool.GetStrand(),
                                                             MakeLoopbackEndpoint());
        ASSERT_TRUE(acceptorResult.has_value()) << ErrorMessage(acceptorResult);
        const auto& acceptor = *acceptorResult;

        const auto startResult = Await(Start(acceptor));
        const auto repeatedStartResult = Await(Start(acceptor));
        const auto stopError = Await(Stop(acceptor));

        EXPECT_TRUE(startResult.has_value()) << ErrorMessage(startResult);
        EXPECT_TRUE(stopError.has_value()) << ErrorMessage(stopError);
        ASSERT_FALSE(repeatedStartResult.has_value());
        EXPECT_EQ(repeatedStartResult.error(), MakeErrorCode(Error::AcceptorNotInitialized));
    }

    TEST(AcceptorStartAcceptingTest, RejectsStartAfterStop)
    {
        IoContextPool pool(SingleThreadCount);
        ASSERT_TRUE(pool.Run().has_value());
        const auto acceptorResult = Acceptor::CreateAcceptor(pool.GetStrand(),
                                                             MakeLoopbackEndpoint());
        ASSERT_TRUE(acceptorResult.has_value()) << ErrorMessage(acceptorResult);
        const auto& acceptor = *acceptorResult;

        const auto stopError = Await(Stop(acceptor));
        const auto startResult = Await(Start(acceptor));

        EXPECT_TRUE(stopError.has_value()) << ErrorMessage(stopError);
        ASSERT_FALSE(startResult.has_value());
        EXPECT_EQ(startResult.error(), MakeErrorCode(Error::AcceptorNotInitialized));
    }

    TEST(AcceptorStartAcceptingTest, ListeningAcceptorAllowsClientConnection)
    {
        IoContextPool pool(SingleThreadCount);
        ASSERT_TRUE(pool.Run().has_value());
        const auto endpoint = ReserveLoopbackEndpoint();
        const auto acceptorResult = Acceptor::CreateAcceptor(pool.GetStrand(), endpoint);
        ASSERT_TRUE(acceptorResult.has_value()) << ErrorMessage(acceptorResult);
        const auto& acceptor = *acceptorResult;
        boost::asio::io_context clientContext;
        Tcp::socket client(clientContext);
        boost::system::error_code connectError;

        const auto startResult = Await(Start(acceptor));
        EXPECT_TRUE(startResult.has_value()) << ErrorMessage(startResult);

        connectError = client.connect(endpoint, connectError);

        boost::system::error_code closeError;
        closeError = client.close(closeError);

        const auto stopError = Await(Stop(acceptor));
        const auto stopResult = pool.Stop();

        EXPECT_FALSE(connectError) << connectError.message();
        EXPECT_FALSE(closeError) << closeError.message();
        EXPECT_TRUE(stopError.has_value()) << ErrorMessage(stopError);
        EXPECT_TRUE(stopResult.has_value()) << ErrorMessage(stopResult);
    }

    TEST(AcceptorStopAcceptingTest, ClosesListeningSocket)
    {
        IoContextPool pool(SingleThreadCount);
        ASSERT_TRUE(pool.Run().has_value());
        const auto endpoint = ReserveLoopbackEndpoint();
        const auto acceptorResult = Acceptor::CreateAcceptor(pool.GetStrand(), endpoint);
        ASSERT_TRUE(acceptorResult.has_value()) << ErrorMessage(acceptorResult);
        const auto& acceptor = *acceptorResult;
        boost::asio::io_context clientContext;
        Tcp::socket client(clientContext);
        boost::system::error_code connectError;

        const auto stopError = Await(Stop(acceptor));
        connectError = client.connect(endpoint, connectError);

        EXPECT_TRUE(stopError.has_value()) << ErrorMessage(stopError);
        EXPECT_TRUE(connectError);
    }

    TEST(AcceptorStopAcceptingTest, DisallowsRepeatedStop)
    {
        IoContextPool pool(SingleThreadCount);
        ASSERT_TRUE(pool.Run().has_value());
        const auto acceptorResult = Acceptor::CreateAcceptor(pool.GetStrand(),
                                                             MakeLoopbackEndpoint());
        ASSERT_TRUE(acceptorResult.has_value()) << ErrorMessage(acceptorResult);
        const auto& acceptor = *acceptorResult;

        const auto firstStopError = Await(Stop(acceptor));
        const auto secondStopError = Await(Stop(acceptor));

        EXPECT_TRUE(firstStopError.has_value()) << ErrorMessage(firstStopError);
        EXPECT_FALSE(secondStopError.has_value()) << ErrorMessage(secondStopError);
    }

    TEST(AcceptorAsyncTest, QueuesCommandsAndKeepsObjectAliveUntilCompletion)
    {
        boost::asio::io_context context;
        const auto strand = boost::asio::make_strand(context);
        auto result = Acceptor::CreateAcceptor(strand, MakeLoopbackEndpoint());
        ASSERT_TRUE(result.has_value());
        auto acceptor = std::move(*result);
        const std::weak_ptr<Acceptor> weakAcceptor = acceptor;
        auto started = Start(acceptor);
        auto stopped = Stop(acceptor);

        EXPECT_EQ(started.wait_for(0s), std::future_status::timeout);
        EXPECT_EQ(stopped.wait_for(0s), std::future_status::timeout);
        acceptor.reset();
        EXPECT_FALSE(weakAcceptor.expired());

        context.run();

        EXPECT_TRUE(Await(std::move(started)).has_value());
        EXPECT_TRUE(Await(std::move(stopped)).has_value());
        EXPECT_TRUE(weakAcceptor.expired());
    }

    TEST(AcceptorAsyncTest, CanStopFromStartCompletionOnTheStrand)
    {
        boost::asio::io_context context;
        const auto strand = boost::asio::make_strand(context);
        const auto result = Acceptor::CreateAcceptor(strand, MakeLoopbackEndpoint());
        ASSERT_TRUE(result.has_value());
        const auto& acceptor = *result;
        bool stopped = false;

        acceptor->AsyncStartAccepting([&](Result startResult) {
            EXPECT_TRUE(strand.running_in_this_thread());
            EXPECT_TRUE(startResult.has_value());
            acceptor->AsyncStopAccepting([&](Result stopResult) {
                EXPECT_TRUE(strand.running_in_this_thread());
                EXPECT_TRUE(stopResult.has_value());
                stopped = true;
            });
        });

        context.run();
        EXPECT_TRUE(stopped);
    }

    TEST(AcceptorAsyncTest, SerializesConcurrentStartAndStopRequests)
    {
        IoContextPool pool(4U);
        ASSERT_TRUE(pool.Run().has_value());
        const auto result = Acceptor::CreateAcceptor(pool.GetStrand(), MakeLoopbackEndpoint());
        ASSERT_TRUE(result.has_value());
        const auto& acceptor = *result;
        constexpr std::size_t RequestCount = 8U;
        std::vector<std::future<Result>> starts(RequestCount);
        std::vector<std::future<Result>> stops(RequestCount);

        {
            std::vector<std::jthread> callers;
            for (std::size_t i = 0; i < RequestCount; ++i)
                callers.emplace_back([&, i] { starts[i] = Start(acceptor); });
        }
        std::size_t successfulStarts = 0U;
        for (auto& future : starts)
        {
            const auto startResult = Await(std::move(future));
            if (startResult)
                ++successfulStarts;
            else
                EXPECT_EQ(startResult.error(), MakeErrorCode(Error::AcceptorNotInitialized));
        }
        EXPECT_EQ(successfulStarts, 1U);

        {
            std::vector<std::jthread> callers;
            for (std::size_t i = 0; i < RequestCount; ++i)
                callers.emplace_back([&, i] { stops[i] = Stop(acceptor); });
        }
        std::size_t successfulStops = 0U;
        for (auto& future : stops)
        {
            const auto stopResult = Await(std::move(future));
            if (stopResult)
                ++successfulStops;
            else
                EXPECT_EQ(stopResult.error(), MakeErrorCode(Error::AcceptorNotStarted));
        }
        EXPECT_EQ(successfulStops, 1U);
        EXPECT_TRUE(pool.Stop().has_value());
    }
}   // namespace Flux::Test
