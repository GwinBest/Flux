#include "Core/Acceptor.hpp"

#include "gtest/gtest.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <expected>
#include <future>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "../Support/Async.hpp"
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

        auto Start(const std::shared_ptr<Acceptor>& acceptor) -> std::future<Result>
        {
            return acceptor->AsyncStartAccepting();
        }

        auto Stop(const std::shared_ptr<Acceptor>& acceptor) -> std::future<Result>
        {
            return acceptor->AsyncStopAccepting();
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

        auto ExpectClientServedUntilEof(boost::asio::io_context& context,
                                        const Tcp::endpoint& endpoint) -> void
        {
            Tcp::socket client(context);
            client.connect(endpoint);
            ASSERT_GT(context.run_one_for(3s), 0U);
            context.poll();
            client.non_blocking(true);
            char probe = 0;
            ErrorCode probeError;
            EXPECT_EQ(client.read_some(boost::asio::buffer(&probe, 1), probeError), 0U);
            ASSERT_TRUE(probeError == boost::asio::error::would_block
                        || probeError == boost::asio::error::try_again)
                << probeError.message();
            client.non_blocking(false);
            constexpr std::string_view payload = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
            auto written = boost::asio::async_write(client,
                                                    boost::asio::buffer(payload),
                                                    boost::asio::use_future);
            ASSERT_EQ(AwaitFuture(context, std::move(written)), payload.size());
            client.shutdown(Tcp::socket::shutdown_send);
            bool readCompleted = false;
            ErrorCode readError;
            std::size_t bytesRead = 0;
            char byte = '\0';
            client.async_read_some(boost::asio::buffer(&byte, 1),
                                   [&](const ErrorCode& error, const std::size_t size) -> void {
                                       readError = error;
                                       bytesRead = size;
                                       readCompleted = true;
                                   });
            // The managed connection drains client data, observes EOF and closes its socket.
            ASSERT_TRUE(PumpUntil(context, [&] -> bool { return readCompleted; }));
            EXPECT_EQ(bytesRead, 0U);
            EXPECT_EQ(readError, boost::asio::error::eof);
            ErrorCode closeError;
            closeError = client.close(closeError);
            EXPECT_FALSE(closeError) << closeError.message();
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

    TEST(AcceptorFactoryTest, ConnectionLimitAccessorHonorsZeroOneAndMaximumCaps)
    {
        boost::asio::io_context context;
        const auto strand = boost::asio::make_strand(context);
        constexpr auto maximum = std::numeric_limits<std::size_t>::max();

        for (const auto cap : {std::size_t {0}, std::size_t {1}, maximum})
        {
            SCOPED_TRACE(cap);
            const auto result = Acceptor::CreateAcceptor(strand, MakeLoopbackEndpoint(), cap);
            ASSERT_TRUE(result.has_value()) << ErrorMessage(result);
            EXPECT_LE((*result)->GetMaxConnections(), cap);
            if (cap == 0)
            {
                EXPECT_EQ((*result)->GetMaxConnections(), 0U);
            }
        }
    }

    TEST(AcceptorFactoryTest, RejectsNonPositiveConnectionIdleTimeout)
    {
        boost::asio::io_context context;
        const auto strand = boost::asio::make_strand(context);

        for (const auto timeout :
             {std::chrono::seconds::min(), std::chrono::seconds::zero(), std::chrono::seconds {-1}})
        {
            SCOPED_TRACE(timeout.count());
            const ConnectionOptions options {.IdleTimeout = timeout};
            EXPECT_THROW(
                {
                    [[maybe_unused]] const auto result = Acceptor::CreateAcceptor(
                        strand,
                        MakeLoopbackEndpoint(),
                        std::nullopt,
                        options);
                },
                std::invalid_argument);
        }
    }

    TEST(AcceptorFactoryTest, AcceptsMinimumAndMaximumPositiveConnectionIdleTimeouts)
    {
        boost::asio::io_context context;
        const auto strand = boost::asio::make_strand(context);

        for (const auto timeout : {std::chrono::seconds {1}, std::chrono::seconds::max()})
        {
            SCOPED_TRACE(timeout.count());
            const ConnectionOptions options {.IdleTimeout = timeout};
            const auto result = Acceptor::CreateAcceptor(strand,
                                                         MakeLoopbackEndpoint(),
                                                         std::size_t {1},
                                                         options);
            ASSERT_TRUE(result.has_value()) << ErrorMessage(result);
            EXPECT_EQ((*result)->GetMaxConnections(), 1U);
        }
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
        ASSERT_FALSE(secondStopError.has_value());
        EXPECT_EQ(secondStopError.error(), MakeErrorCode(Error::AcceptorNotStarted));
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

        EXPECT_GT(context.run(), 0U);

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
        bool commandsCompleted = false;
        const auto& acceptor = *result;

        boost::asio::post(strand, [&] -> void {
            auto started = acceptor->AsyncStartAccepting();
            ASSERT_EQ(started.wait_for(0s), std::future_status::ready);
            EXPECT_TRUE(started.get().has_value());

            auto stopped = acceptor->AsyncStopAccepting();
            ASSERT_EQ(stopped.wait_for(0s), std::future_status::ready);
            EXPECT_TRUE(stopped.get().has_value());
            commandsCompleted = true;
        });

        EXPECT_GT(context.run(), 0U);
        EXPECT_TRUE(commandsCompleted);
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
        EXPECT_GT(context.run(), 0U);
        EXPECT_TRUE(context.stopped());
    }

    TEST(AcceptorAsyncTest, StopClosesActiveManagedConnections)
    {
        boost::asio::io_context context;
        const auto endpoint = ReserveLoopbackEndpoint();
        const auto result = Acceptor::CreateAcceptor(boost::asio::make_strand(context), endpoint);
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(AwaitFuture(context, Start(*result)).has_value());
        std::vector<Tcp::socket> clients;
        for (int index = 0; index < 3; ++index)
        {
            clients.emplace_back(context);
            clients.back().connect(endpoint);
            ASSERT_GT(context.run_one_for(3s), 0U);
            context.poll();
        }
        ASSERT_TRUE(AwaitFuture(context, Stop(*result)).has_value());
        for (auto& client : clients)
        {
            bool completed = false;
            ErrorCode readError;
            char byte = 0;
            client.async_read_some(boost::asio::buffer(&byte, 1),
                                   [&](const ErrorCode& error, std::size_t size) -> void {
                                       readError = error;
                                       EXPECT_EQ(size, 0U);
                                       completed = true;
                                   });
            ASSERT_TRUE(PumpUntil(context, [&] -> bool { return completed; }));
            EXPECT_TRUE(readError == boost::asio::error::eof
                        || readError == boost::asio::error::connection_reset)
                << readError.message();
        }
        context.restart();
        context.poll();
        EXPECT_TRUE(context.stopped());
    }

    TEST(AcceptorAsyncTest, DestroyedAcceptorAllowsActiveClientToFinishSafely)
    {
        boost::asio::io_context context;
        const auto endpoint = ReserveLoopbackEndpoint();
        auto result = Acceptor::CreateAcceptor(boost::asio::make_strand(context), endpoint);
        ASSERT_TRUE(result.has_value());
        auto acceptor = std::move(*result);
        ASSERT_TRUE(AwaitFuture(context, Start(acceptor)).has_value());
        Tcp::socket client(context);
        client.connect(endpoint);
        ASSERT_GT(context.run_one_for(3s), 0U);
        context.poll();
        const std::weak_ptr weakAcceptor = acceptor;
        acceptor.reset();
        EXPECT_TRUE(weakAcceptor.expired());
        client.shutdown(Tcp::socket::shutdown_send);
        bool completed = false;
        ErrorCode readError;
        char byte = 0;
        client.async_read_some(boost::asio::buffer(&byte, 1),
                               [&](const ErrorCode& error, std::size_t) -> void {
                                   readError = error;
                                   completed = true;
                               });
        ASSERT_TRUE(PumpUntil(context, [&] -> bool { return completed; }));
        EXPECT_TRUE(readError == boost::asio::error::eof
                    || readError == boost::asio::error::connection_reset)
            << readError.message();
        context.restart();
        context.poll();
        EXPECT_TRUE(context.stopped());
    }

    TEST(AcceptorAsyncTest, ManagesSeveralClientsUntilEachPeerCloses)
    {
        boost::asio::io_context context;
        const auto endpoint = ReserveLoopbackEndpoint();
        const auto result = Acceptor::CreateAcceptor(boost::asio::make_strand(context), endpoint);
        ASSERT_TRUE(result.has_value());
        auto started = Start(*result);
        ASSERT_EQ(context.poll_one(), 1U);
        ASSERT_TRUE(Await(std::move(started)).has_value());

        for (int index = 0; index < 3; ++index)
        {
            SCOPED_TRACE(index);
            ExpectClientServedUntilEof(context, endpoint);
        }

        auto stopped = Stop(*result);
        EXPECT_GT(context.run_for(3s), 0U);
        EXPECT_TRUE(Await(std::move(stopped)).has_value());
        EXPECT_TRUE(context.stopped());
    }
}   // namespace Flux::Test
