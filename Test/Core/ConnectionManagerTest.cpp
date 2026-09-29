#include "Core/ConnectionManager.hpp"
#include "gtest/gtest.h"

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/post.hpp>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "../Support/Async.hpp"
#include "Core/Connection.hpp"
#include "Core/IoContextPool.hpp"

namespace Flux::Test
{
    static_assert(!std::is_copy_constructible_v<ConnectionManager>);
    static_assert(!std::is_move_constructible_v<ConnectionManager>);

    TEST(ConnectionManagerTest, EmptyAndForeignConnectionsAreNoOps)
    {
        boost::asio::io_context context;
        ConnectionManager manager;
        ConnectionManager other;
        const auto foreign = other.CreateConnection(boost::asio::ip::tcp::socket(context));
        EXPECT_TRUE(manager.CloseConnection(nullptr).has_value());
        EXPECT_TRUE(manager.CloseConnection(foreign).has_value());
        EXPECT_TRUE(manager.CloseAllConnections().has_value());
        const auto result = AwaitFuture(context, foreign->Send({}));
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), MakeErrorCode(Error::ConnectionNotStarted));
        EXPECT_TRUE(AwaitFuture(context, foreign->AsyncClose()).has_value());
    }

    TEST(ConnectionManagerTest, RetainsCreatedConnectionUntilExplicitClose)
    {
        boost::asio::io_context context;
        ConnectionManager manager;
        auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        const std::weak_ptr weakConnection = connection;
        connection.reset();
        ASSERT_FALSE(weakConnection.expired());
        const auto closed = OnExecutor(context, context.get_executor(), [&] {
            return manager.CloseConnection(weakConnection.lock());
        });
        EXPECT_TRUE(closed.has_value());
        EXPECT_TRUE(weakConnection.expired());
    }

    TEST(ConnectionManagerTest, DirectCloseRemovesConnectionFromManager)
    {
        boost::asio::io_context context;
        ConnectionManager manager;
        auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        const std::weak_ptr weakConnection = connection;
        ASSERT_TRUE(AwaitFuture(context, connection->AsyncClose()).has_value());
        connection.reset();
        EXPECT_TRUE(weakConnection.expired());
        EXPECT_TRUE(manager.CloseAllConnections().has_value());
    }

    TEST(ConnectionManagerTest, DestructionReleasesUnstartedConnections)
    {
        boost::asio::io_context context;
        std::weak_ptr<Connection> connection;
        {
            ConnectionManager manager;
            connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
            EXPECT_FALSE(connection.expired());
        }
        EXPECT_TRUE(connection.expired());
    }

    TEST(ConnectionManagerTest, CloseFromControlThreadRunsCallbackOnWorker)
    {
        IoContextPool pool(1);
        ASSERT_TRUE(pool.Run().has_value());
        ConnectionManager manager;
        auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(pool.GetStrand()));
        const std::weak_ptr weakConnection = connection;
        EXPECT_TRUE(manager.CloseConnection(connection).has_value());
        connection.reset();
        EXPECT_TRUE(pool.Stop().has_value());
        EXPECT_TRUE(weakConnection.expired());
    }

    TEST(ConnectionManagerTest, ConcurrentCreationAndCloseReleaseEveryConnection)
    {
        IoContextPool pool(2);
        ASSERT_TRUE(pool.Run().has_value());
        ConnectionManager manager;
        std::vector<std::jthread> callers;
        std::vector<std::weak_ptr<Connection>> connections(80);
        for (std::size_t worker = 0; worker < 4; ++worker)
        {
            callers.emplace_back([&, worker] {
                for (std::size_t index = 0; index < 20; ++index)
                {
                    auto connection = manager.CreateConnection(
                        boost::asio::ip::tcp::socket(pool.GetStrand()));
                    connections[worker * 20 + index] = connection;
                    EXPECT_TRUE(manager.CloseConnection(connection).has_value());
                    connection.reset();
                }
            });
        }
        callers.clear();
        EXPECT_TRUE(manager.CloseAllConnections().has_value());
        EXPECT_TRUE(pool.Stop().has_value());
        for (const auto& connection : connections) EXPECT_TRUE(connection.expired());
    }

    TEST(ConnectionManagerTest, BulkCloseShutsDownActivePeersAndReleasesConnections)
    {
        using Tcp = boost::asio::ip::tcp;
        boost::asio::io_context clients;
        Tcp::acceptor listener(clients, {boost::asio::ip::address_v4::loopback(), 0});
        IoContextPool pool(2);
        ASSERT_TRUE(pool.Run().has_value());
        ConnectionManager manager;
        std::vector<Tcp::socket> peers;
        std::vector<std::weak_ptr<Connection>> connections;
        for (int index = 0; index < 5; ++index)
        {
            peers.emplace_back(clients);
            peers.back().connect(listener.local_endpoint());
            Tcp::socket accepted(pool.GetStrand());
            listener.accept(accepted);
            auto result = manager.CreateAndStartConnection(std::move(accepted));
            ASSERT_TRUE(result.has_value());
            connections.emplace_back(*result);
        }
        EXPECT_TRUE(manager.CloseAllConnections().has_value());
        EXPECT_TRUE(pool.Stop().has_value());
        for (const auto& connection : connections) EXPECT_TRUE(connection.expired());
        for (auto& peer : peers)
        {
            char byte = 0;
            ErrorCode error;
            bool completed = false;
            peer.async_read_some(boost::asio::buffer(&byte, 1),
                                 [&](const ErrorCode& readError, std::size_t size) {
                                     error = readError;
                                     EXPECT_EQ(size, 0U);
                                     completed = true;
                                 });
            ASSERT_TRUE(PumpUntil(clients, [&] { return completed; }));
            EXPECT_TRUE(error == boost::asio::error::eof
                        || error == boost::asio::error::connection_reset)
                << error.message();
        }
    }

    TEST(ConnectionManagerTest, StartReturnsErrorForUnopenedSocket)
    {
        boost::asio::io_context context;
        ConnectionManager manager;

        boost::asio::post(context, [&] {
            const auto result = manager.CreateAndStartConnection(
                boost::asio::ip::tcp::socket(context));

            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), ErrorCode(boost::asio::error::bad_descriptor));
            EXPECT_TRUE(manager.CloseAllConnections().has_value());
        });
        context.run();
    }

    TEST(ConnectionManagerTest, StartsAndClosesConnectedSocket)
    {
        using Tcp = boost::asio::ip::tcp;
        boost::asio::io_context context;
        Tcp::acceptor acceptor(context, {boost::asio::ip::address_v4::loopback(), 0});
        Tcp::socket client(context);
        client.connect(acceptor.local_endpoint());
        auto socket = acceptor.accept();
        ConnectionManager manager;

        boost::asio::post(context, [&] {
            auto result = manager.CreateAndStartConnection(std::move(socket));
            ASSERT_TRUE(result.has_value());
            const auto connection = *result;

            EXPECT_TRUE(manager.CloseConnection(connection).has_value());
            const auto restartResult = connection->AsyncStart().get();
            ASSERT_FALSE(restartResult.has_value());
            EXPECT_EQ(restartResult.error(), MakeErrorCode(Error::ConnectionClosed));

            EXPECT_TRUE(manager.CloseConnection(connection).has_value());
        });
        context.run();
    }

    TEST(ConnectionManagerTest, CloseAllClosesEveryConnectionAndReleasesOwnership)
    {
        boost::asio::io_context context;
        ConnectionManager manager;
        auto first = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        auto second = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        const std::weak_ptr<Connection> weakFirst = first;
        const std::weak_ptr<Connection> weakSecond = second;

        boost::asio::post(context, [&] {
            EXPECT_TRUE(manager.CloseAllConnections().has_value());
            const auto firstStart = first->AsyncStart().get();
            const auto secondStart = second->AsyncStart().get();
            ASSERT_FALSE(firstStart.has_value());
            ASSERT_FALSE(secondStart.has_value());
            EXPECT_EQ(firstStart.error(), MakeErrorCode(Error::ConnectionClosed));
            EXPECT_EQ(secondStart.error(), MakeErrorCode(Error::ConnectionClosed));
        });
        context.run();

        first.reset();
        second.reset();
        EXPECT_TRUE(weakFirst.expired());
        EXPECT_TRUE(weakSecond.expired());
        EXPECT_TRUE(manager.CloseAllConnections().has_value());
    }
}   // namespace Flux::Test
