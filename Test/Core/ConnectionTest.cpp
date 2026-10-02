#include "Core/Connection.hpp"
#include "gtest/gtest.h"

#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/write.hpp>
#include <chrono>
#include <memory>
#include <vector>

#include "../Support/Async.hpp"
#include "Core/ConnectionManager.hpp"

namespace Flux::Test
{
    namespace
    {
        using Tcp = boost::asio::ip::tcp;
        using namespace std::chrono_literals;

        class ConnectionTest : public testing::Test
        {
        protected:
            boost::asio::io_context context;
            boost::asio::strand<boost::asio::io_context::executor_type>
                strand = boost::asio::make_strand(context);
            ConnectionManager manager {1};
            Tcp::socket peer {context};
            ConnectionManager::ConnectionPtr connection;

            auto SetUp() -> void override
            {
                Tcp::acceptor listener(context, {boost::asio::ip::address_v4::loopback(), 0});
                peer.connect(listener.local_endpoint());
                Tcp::socket socket(strand);
                listener.accept(socket);
                auto result = manager.CreateConnection(std::move(socket));
                ASSERT_TRUE(result.has_value());
                connection = std::move(*result);
                ASSERT_NE(connection, nullptr);
            }

            auto TearDown() -> void override
            {
                if (connection)
                {
                    EXPECT_TRUE(AwaitFuture(context, connection->AsyncClose()).has_value());
                }
                context.restart();
                context.poll();
            }

            auto Start() -> void
            {
                ASSERT_TRUE(AwaitFuture(context, connection->AsyncStart()).has_value());
            }
        };
    }   // namespace

    TEST_F(ConnectionTest, RejectsSendBeforeStartIncludingEmptyPayload)
    {
        for (auto data : {std::vector<std::byte> {}, std::vector {std::byte {1}}})
        {
            const auto result = AwaitFuture(context, connection->Send(std::move(data)));
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), MakeErrorCode(Error::ConnectionNotStarted));
        }
    }

    TEST_F(ConnectionTest, StartIsQueuedAndRepeatedStartIsRejected)
    {
        auto started = connection->AsyncStart();
        EXPECT_EQ(started.wait_for(0s), std::future_status::timeout);
        ASSERT_TRUE(AwaitFuture(context, std::move(started)).has_value());
        const auto repeated = AwaitFuture(context, connection->AsyncStart());
        ASSERT_FALSE(repeated.has_value());
        EXPECT_EQ(repeated.error(), MakeErrorCode(Error::ConnectionAlreadyStarted));
    }

    TEST_F(ConnectionTest, CommandsCompleteInlineOnTheConnectionStrand)
    {
        OnExecutor(context, strand, [&] {
            auto started = connection->AsyncStart();
            ASSERT_EQ(started.wait_for(0s), std::future_status::ready);
            EXPECT_TRUE(started.get().has_value());
            auto sent = connection->Send({});
            ASSERT_EQ(sent.wait_for(0s), std::future_status::ready);
            EXPECT_TRUE(sent.get().has_value());
            auto closed = connection->AsyncClose();
            ASSERT_EQ(closed.wait_for(0s), std::future_status::ready);
            EXPECT_TRUE(closed.get().has_value());
        });
    }

    TEST_F(ConnectionTest, QueuedCommandsObserveLifecycleOrder)
    {
        auto beforeStart = connection->Send({});
        auto started = connection->AsyncStart();
        auto afterStart = connection->Send({});
        auto closed = connection->AsyncClose();
        auto afterClose = connection->Send({std::byte {1}});
        EXPECT_EQ(beforeStart.wait_for(0s), std::future_status::timeout);
        EXPECT_EQ(started.wait_for(0s), std::future_status::timeout);
        EXPECT_EQ(afterStart.wait_for(0s), std::future_status::timeout);
        EXPECT_EQ(closed.wait_for(0s), std::future_status::timeout);
        EXPECT_EQ(afterClose.wait_for(0s), std::future_status::timeout);

        const auto earlySend = AwaitFuture(context, std::move(beforeStart));
        ASSERT_FALSE(earlySend.has_value());
        EXPECT_EQ(earlySend.error(), MakeErrorCode(Error::ConnectionNotStarted));
        EXPECT_TRUE(AwaitFuture(context, std::move(started)).has_value());
        EXPECT_TRUE(AwaitFuture(context, std::move(afterStart)).has_value());
        EXPECT_TRUE(AwaitFuture(context, std::move(closed)).has_value());
        const auto lateSend = AwaitFuture(context, std::move(afterClose));
        ASSERT_FALSE(lateSend.has_value());
        EXPECT_EQ(lateSend.error(), MakeErrorCode(Error::ConnectionClosed));
    }

    TEST_F(ConnectionTest, EmptySendSucceedsAndDoesNotPreventNextWrite)
    {
        Start();
        ASSERT_TRUE(AwaitFuture(context, connection->Send({})).has_value());
        const std::vector expected {std::byte {12}, std::byte {34}};
        std::vector<std::byte> received(expected.size());
        auto read = boost::asio::async_read(peer,
                                            boost::asio::buffer(received),
                                            boost::asio::use_future);
        EXPECT_TRUE(AwaitFuture(context, connection->Send(std::vector(expected))).has_value());
        EXPECT_EQ(AwaitFuture(context, std::move(read)), expected.size());
        EXPECT_EQ(received, expected);
    }

    TEST_F(ConnectionTest, QueuedWritesPreserveOrderAndOwnMovedPayloads)
    {
        Start();
        std::vector<std::byte> expected;
        std::vector<std::vector<std::byte>> messages;
        for (unsigned char index = 1; index <= 8; ++index)
        {
            messages.emplace_back(32768, static_cast<std::byte>(index));
            expected.insert(expected.end(), messages.back().begin(), messages.back().end());
        }
        // All sends run inside one strand handler, before any write completion can run.
        OnExecutor(context, strand, [&] {
            for (auto& message : messages)
                EXPECT_TRUE(connection->Send(std::move(message)).get().has_value());
        });
        messages.clear();

        std::vector<std::byte> received(expected.size());
        auto read = boost::asio::async_read(peer,
                                            boost::asio::buffer(received),
                                            boost::asio::use_future);
        EXPECT_EQ(AwaitFuture(context, std::move(read)), expected.size());
        EXPECT_EQ(received, expected);
    }

    TEST_F(ConnectionTest, ReadsMoreThanOneBufferAndClosesOnPeerEof)
    {
        Start();
        const std::vector<std::byte> data(128 * 1024, std::byte {42});
        auto write = boost::asio::async_write(peer,
                                              boost::asio::buffer(data),
                                              boost::asio::use_future);
        ASSERT_EQ(AwaitFuture(context, std::move(write)), data.size());
        peer.shutdown(Tcp::socket::shutdown_send);
        const std::weak_ptr weakConnection = connection;
        connection.reset();
        // Expiration requires reading through the stream to EOF and removal by the manager.
        ASSERT_TRUE(PumpUntil(context, [&] { return weakConnection.expired(); }));
        const auto replacement = manager.CreateConnection(Tcp::socket(strand));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_TRUE(AwaitFuture(context, (*replacement)->AsyncClose()).has_value());
    }

    TEST_F(ConnectionTest, PeerResetClosesAndReleasesConnection)
    {
        Start();
        peer.set_option(boost::asio::socket_base::linger(true, 0));
        peer.close();
        const std::weak_ptr weakConnection = connection;
        connection.reset();
        ASSERT_TRUE(PumpUntil(context, [&] { return weakConnection.expired(); }));
        const auto replacement = manager.CreateConnection(Tcp::socket(strand));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_TRUE(AwaitFuture(context, (*replacement)->AsyncClose()).has_value());
    }

    TEST_F(ConnectionTest, CloseBeforeStartIsIdempotentAndRejectsFurtherOperations)
    {
        EXPECT_TRUE(AwaitFuture(context, connection->AsyncClose()).has_value());
        EXPECT_TRUE(AwaitFuture(context, connection->AsyncClose()).has_value());
        const auto start = AwaitFuture(context, connection->AsyncStart());
        const auto send = AwaitFuture(context, connection->Send({std::byte {1}}));
        const auto emptySend = AwaitFuture(context, connection->Send({}));
        ASSERT_FALSE(start.has_value());
        ASSERT_FALSE(send.has_value());
        ASSERT_FALSE(emptySend.has_value());
        EXPECT_EQ(start.error(), MakeErrorCode(Error::ConnectionClosed));
        EXPECT_EQ(send.error(), MakeErrorCode(Error::ConnectionClosed));
        EXPECT_EQ(emptySend.error(), MakeErrorCode(Error::ConnectionClosed));
    }

    TEST_F(ConnectionTest, CloseCancelsPendingReadAndQueuedWritesAndReleasesBuffers)
    {
        Start();
        OnExecutor(context, strand, [&] {
            EXPECT_TRUE(connection->Send(std::vector<std::byte>(4 * 1024 * 1024, std::byte {1}))
                            .get()
                            .has_value());
            EXPECT_TRUE(
                connection->Send(std::vector<std::byte>(1024, std::byte {2})).get().has_value());
            EXPECT_TRUE(connection->AsyncClose().get().has_value());
        });
        const std::weak_ptr weakConnection = connection;
        connection.reset();
        EXPECT_TRUE(PumpUntil(context, [&] { return weakConnection.expired(); }));
    }

    TEST(ConnectionAsyncTest, WriteFailureClosesAndUnregistersConnection)
    {
        boost::asio::io_context context;
        const auto strand = boost::asio::make_strand(context);
        Tcp::acceptor listener(context, {boost::asio::ip::address_v4::loopback(), 0});
        Tcp::socket peer(context);
        peer.connect(listener.local_endpoint());
        Tcp::socket socket(strand);
        listener.accept(socket);
        // A real TCP half-close makes writing fail while reading remains possible.
        socket.shutdown(Tcp::socket::shutdown_send);
        const ConnectionManager manager(1);
        auto connection = manager.CreateConnection(std::move(socket));
        ASSERT_TRUE(connection.has_value());
        ASSERT_TRUE(AwaitFuture(context, connection.value()->AsyncStart()).has_value());
        ASSERT_TRUE(AwaitFuture(context, connection.value()->Send({std::byte {1}})).has_value());
        const std::weak_ptr weakConnection = connection.value();
        connection.value().reset();
        ASSERT_TRUE(PumpUntil(context, [&] { return weakConnection.expired(); }));
        const auto replacement = manager.CreateConnection(Tcp::socket(strand));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_TRUE(AwaitFuture(context, (*replacement)->AsyncClose()).has_value());
    }

    TEST(ConnectionAsyncTest, QueuedOperationsKeepConnectionAliveAfterManagerDestruction)
    {
        boost::asio::io_context context;
        const auto strand = boost::asio::make_strand(context);
        auto manager = std::make_unique<ConnectionManager>(1);
        auto connection = manager->CreateConnection(Tcp::socket(strand));
        ASSERT_TRUE(connection.has_value());
        const std::weak_ptr weakConnection = connection.value();
        auto start = connection.value()->AsyncStart();
        auto close = connection.value()->AsyncClose();
        manager.reset();
        connection.value().reset();
        EXPECT_FALSE(weakConnection.expired());
        const auto startResult = AwaitFuture(context, std::move(start));
        ASSERT_FALSE(startResult.has_value());
        EXPECT_EQ(startResult.error(), ErrorCode(boost::asio::error::bad_descriptor));
        EXPECT_TRUE(AwaitFuture(context, std::move(close)).has_value());
        EXPECT_TRUE(PumpUntil(context, [&] { return weakConnection.expired(); }));
    }

    TEST(ConnectionAsyncTest, FailedStartRemainsUnstartedUntilExplicitClose)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto connection = manager.CreateConnection(Tcp::socket(context));
        ASSERT_TRUE(connection.has_value());
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            const auto started = AwaitFuture(context, (*connection)->AsyncStart());
            ASSERT_FALSE(started.has_value());
            EXPECT_EQ(started.error(), ErrorCode(boost::asio::error::bad_descriptor));
        }
        const auto sent = AwaitFuture(context, (*connection)->Send({}));
        ASSERT_FALSE(sent.has_value());
        EXPECT_EQ(sent.error(), MakeErrorCode(Error::ConnectionNotStarted));
        ASSERT_TRUE(AwaitFuture(context, (*connection)->AsyncClose()).has_value());
        const auto restarted = AwaitFuture(context, (*connection)->AsyncStart());
        ASSERT_FALSE(restarted.has_value());
        EXPECT_EQ(restarted.error(), MakeErrorCode(Error::ConnectionClosed));
    }

    TEST(ConnectionAsyncTest, ActiveConnectionRemainsUsableAfterManagerDestruction)
    {
        boost::asio::io_context context;
        Tcp::acceptor listener(context, {boost::asio::ip::address_v4::loopback(), 0});
        Tcp::socket peer(context);
        peer.connect(listener.local_endpoint());
        ConnectionManager::ConnectionPtr connection;
        {
            const ConnectionManager manager(1);
            const auto result = OnExecutor(context, context.get_executor(), [&] {
                return manager.CreateAndStartConnection(listener.accept());
            });
            ASSERT_TRUE(result.has_value());
            connection = *result;
        }
        const std::vector payload {std::byte {1}, std::byte {2}, std::byte {3}};
        std::vector<std::byte> received(payload.size());
        auto read = boost::asio::async_read(peer,
                                            boost::asio::buffer(received),
                                            boost::asio::use_future);
        EXPECT_TRUE(AwaitFuture(context, connection->Send(std::vector(payload))).has_value());
        EXPECT_EQ(AwaitFuture(context, std::move(read)), payload.size());
        EXPECT_EQ(received, payload);
        EXPECT_TRUE(AwaitFuture(context, connection->AsyncClose()).has_value());
        const std::weak_ptr weakConnection = connection;
        connection.reset();
        EXPECT_TRUE(PumpUntil(context, [&] { return weakConnection.expired(); }));
    }
}   // namespace Flux::Test
