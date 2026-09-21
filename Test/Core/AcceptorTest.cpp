#include "Core/Acceptor.hpp"
#include "gtest/gtest.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
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

#include "../Support/Socket.hpp"
#include "Core/IoContextPool.hpp"

namespace Flux::Test
{
    using namespace std::chrono_literals;

    namespace
    {
        using Tcp = boost::asio::ip::tcp;

        constexpr std::size_t SingleThreadCount = 1U;
        constexpr unsigned short AnyPort = 0U;

        struct CompletionObservation
        {
            bool Called = false;
            bool Inline = false;
            bool OnStrand = false;
            Result Outcome;
        };

        auto ExpectInlineSuccess(const CompletionObservation& observation, const char* operation)
            -> void
        {
            SCOPED_TRACE(operation);
            EXPECT_TRUE(observation.Inline);
            EXPECT_TRUE(observation.OnStrand);
            EXPECT_TRUE(observation.Outcome.has_value());
        }

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

        auto ExpectClientConnected(Tcp::socket& client) -> void
        {
            client.non_blocking(true);
            char byte = '\0';
            ErrorCode error;

            const auto size = client.read_some(boost::asio::buffer(&byte, 1), error);

            EXPECT_EQ(size, 0U);
            EXPECT_TRUE(error == boost::asio::error::would_block
                        || error == boost::asio::error::try_again)
                << error.message();
        }

        auto ExpectClientDisconnected(Tcp::socket& client) -> void
        {
            client.non_blocking(true);
            char byte = '\0';
            ErrorCode error;

            const auto size = client.read_some(boost::asio::buffer(&byte, 1), error);

            EXPECT_EQ(size, 0U);
            EXPECT_TRUE(error == boost::asio::error::eof
                        || error == boost::asio::error::connection_reset)
                << error.message();
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
        const std::weak_ptr weakAcceptor = acceptor;
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

    TEST(AcceptorFactoryTest, ReturnsBindErrorForOccupiedEndpoint)
    {
        boost::asio::io_context context;
        Tcp::acceptor occupied(context);
        OccupyEndpoint(occupied);
        const auto result = Acceptor::CreateAcceptor(boost::asio::make_strand(context),
                                                     occupied.local_endpoint());
        ASSERT_FALSE(result.has_value());
        EXPECT_TRUE(result.error() == boost::asio::error::address_in_use
                    || result.error() == boost::asio::error::access_denied)
            << result.error().message();
    }

    TEST(AcceptorAsyncTest, CompletesInlineWhenCalledFromItsStrand)
    {
        boost::asio::io_context context;
        const auto strand = boost::asio::make_strand(context);

        const auto result = Acceptor::CreateAcceptor(strand, MakeLoopbackEndpoint());
        ASSERT_TRUE(result.has_value());

        CompletionObservation started;
        CompletionObservation stopped;
        const auto& acceptor = *result;

        boost::asio::post(strand, [&] -> void {
            acceptor->AsyncStartAccepting([&](const Result& startResult) -> void {
                started.OnStrand = strand.running_in_this_thread();
                started.Outcome = startResult;
                started.Called = true;
            });
            started.Inline = started.Called;

            acceptor->AsyncStopAccepting([&](const Result& stopResult) -> void {
                stopped.OnStrand = strand.running_in_this_thread();
                stopped.Outcome = stopResult;
                stopped.Called = true;
            });
            stopped.Inline = stopped.Called;
        });

        context.run();
        ExpectInlineSuccess(started, "start");
        ExpectInlineSuccess(stopped, "stop");
    }

    TEST(AcceptorAsyncTest, PendingAcceptDoesNotKeepDestroyedAcceptorAlive)
    {
        boost::asio::io_context context;
        auto result = Acceptor::CreateAcceptor(boost::asio::make_strand(context),
                                               MakeLoopbackEndpoint());
        ASSERT_TRUE(result.has_value());

        auto acceptor = std::move(*result);
        const std::weak_ptr weakAcceptor = acceptor;
        auto started = Start(acceptor);

        ASSERT_EQ(context.poll_one(), 1U);
        ASSERT_TRUE(Await(std::move(started)).has_value());

        acceptor.reset();
        EXPECT_TRUE(weakAcceptor.expired());
        context.run();
        EXPECT_TRUE(context.stopped());
    }

    TEST(AcceptorAsyncTest, DestroyingAcceptorClosesAcceptedConnections)
    {
        boost::asio::io_context context;
        const auto endpoint = ReserveLoopbackEndpoint();

        auto result = Acceptor::CreateAcceptor(boost::asio::make_strand(context), endpoint);
        ASSERT_TRUE(result.has_value());

        auto acceptor = std::move(*result);
        auto started = Start(acceptor);

        ASSERT_EQ(context.poll_one(), 1U);
        ASSERT_TRUE(Await(std::move(started)).has_value());

        Tcp::socket client(context);
        client.connect(endpoint);
        context.run_for(50ms);
        ExpectClientConnected(client);

        acceptor.reset();
        context.run_for(3s);
        EXPECT_TRUE(context.stopped());
        ExpectClientDisconnected(client);
    }

    TEST(AcceptorAsyncTest, KeepsAcceptedClientsConnectedUntilStopped)
    {
        boost::asio::io_context context;
        const auto endpoint = ReserveLoopbackEndpoint();

        const auto result = Acceptor::CreateAcceptor(boost::asio::make_strand(context), endpoint);

        ASSERT_TRUE(result.has_value());

        auto started = Start(*result);

        ASSERT_EQ(context.poll_one(), 1U);
        ASSERT_TRUE(Await(std::move(started)).has_value());

        std::vector<Tcp::socket> clients;
        for (int index = 0; index < 3; ++index)
        {
            SCOPED_TRACE(index);
            clients.emplace_back(context);
            clients.back().connect(endpoint);
            context.run_for(50ms);
            ExpectClientConnected(clients.back());
        }
        
        auto stopped = Stop(*result);
        context.run_for(3s);
        EXPECT_TRUE(Await(std::move(stopped)).has_value());
        EXPECT_TRUE(context.stopped());
        for (auto& client : clients) ExpectClientDisconnected(client);
    }
}   // namespace Flux::Test
