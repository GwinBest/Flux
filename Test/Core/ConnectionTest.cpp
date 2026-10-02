#include "Core/Connection.hpp"
#include "gtest/gtest.h"

#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/write.hpp>
#include <chrono>
#include <cstddef>
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
            boost::asio::io_context _context;
            boost::asio::strand<boost::asio::io_context::executor_type>
                _strand = boost::asio::make_strand(_context);
            ConnectionManager _manager {1};
            Tcp::socket _peer {_context};
            ConnectionManager::ConnectionPtr _connection;

            auto SetUp() -> void override
            {
                Tcp::acceptor listener(_context, {boost::asio::ip::address_v4::loopback(), 0});
                _peer.connect(listener.local_endpoint());
                Tcp::socket socket(_strand);
                listener.accept(socket);
                auto result = _manager.CreateConnection(std::move(socket));
                ASSERT_TRUE(result.has_value());
                _connection = std::move(*result);
                ASSERT_NE(_connection, nullptr);
            }

            auto TearDown() -> void override
            {
                if (_connection)
                {
                    EXPECT_TRUE(AwaitFuture(_context, _connection->AsyncClose()).has_value());
                }
                _context.restart();
                _context.poll();
            }

            auto Start() -> void
            {
                ASSERT_TRUE(AwaitFuture(_context, _connection->AsyncStart()).has_value());
            }
        };
    }   // namespace

    TEST_F(ConnectionTest, RejectsSendBeforeStartIncludingEmptyPayload)
    {
        for (auto data : {std::vector<std::byte> {}, std::vector {std::byte {1}}})
        {
            const auto result = AwaitFuture(_context, _connection->Send(std::move(data)));
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), MakeErrorCode(Error::ConnectionNotStarted));
        }
    }

    TEST_F(ConnectionTest, StartIsQueuedAndRepeatedStartIsRejected)
    {
        auto started = _connection->AsyncStart();
        EXPECT_EQ(started.wait_for(0s), std::future_status::timeout);
        ASSERT_TRUE(AwaitFuture(_context, std::move(started)).has_value());
        const auto repeated = AwaitFuture(_context, _connection->AsyncStart());
        ASSERT_FALSE(repeated.has_value());
        EXPECT_EQ(repeated.error(), MakeErrorCode(Error::ConnectionAlreadyStarted));
    }

    TEST_F(ConnectionTest, CommandsCompleteInlineOnTheConnectionStrand)
    {
        OnExecutor(_context, _strand, [&] -> void {
            auto started = _connection->AsyncStart();
            ASSERT_EQ(started.wait_for(0s), std::future_status::ready);
            EXPECT_TRUE(started.get().has_value());
            auto sent = _connection->Send({});
            ASSERT_EQ(sent.wait_for(0s), std::future_status::ready);
            EXPECT_TRUE(sent.get().has_value());
            auto closed = _connection->AsyncClose();
            ASSERT_EQ(closed.wait_for(0s), std::future_status::ready);
            EXPECT_TRUE(closed.get().has_value());
        });
    }

    TEST_F(ConnectionTest, QueuedCommandsObserveLifecycleOrder)
    {
        auto beforeStart = _connection->Send({});
        auto started = _connection->AsyncStart();
        auto afterStart = _connection->Send({});
        auto closed = _connection->AsyncClose();
        auto afterClose = _connection->Send({std::byte {1}});
        EXPECT_EQ(beforeStart.wait_for(0s), std::future_status::timeout);
        EXPECT_EQ(started.wait_for(0s), std::future_status::timeout);
        EXPECT_EQ(afterStart.wait_for(0s), std::future_status::timeout);
        EXPECT_EQ(closed.wait_for(0s), std::future_status::timeout);
        EXPECT_EQ(afterClose.wait_for(0s), std::future_status::timeout);

        const auto earlySend = AwaitFuture(_context, std::move(beforeStart));
        ASSERT_FALSE(earlySend.has_value());
        EXPECT_EQ(earlySend.error(), MakeErrorCode(Error::ConnectionNotStarted));
        EXPECT_TRUE(AwaitFuture(_context, std::move(started)).has_value());
        EXPECT_TRUE(AwaitFuture(_context, std::move(afterStart)).has_value());
        EXPECT_TRUE(AwaitFuture(_context, std::move(closed)).has_value());
        const auto lateSend = AwaitFuture(_context, std::move(afterClose));
        ASSERT_FALSE(lateSend.has_value());
        EXPECT_EQ(lateSend.error(), MakeErrorCode(Error::ConnectionClosed));
    }

    TEST_F(ConnectionTest, EmptySendSucceedsAndDoesNotPreventNextWrite)
    {
        Start();
        ASSERT_TRUE(AwaitFuture(_context, _connection->Send({})).has_value());
        const std::vector expected {std::byte {12}, std::byte {34}};
        std::vector<std::byte> received(expected.size());
        auto read = boost::asio::async_read(_peer,
                                            boost::asio::buffer(received),
                                            boost::asio::use_future);
        EXPECT_TRUE(AwaitFuture(_context, _connection->Send(std::vector(expected))).has_value());
        EXPECT_EQ(AwaitFuture(_context, std::move(read)), expected.size());
        EXPECT_EQ(received, expected);
    }

    TEST_F(ConnectionTest, QueuedWritesPreserveOrderAndOwnMovedPayloads)
    {
        Start();
        std::vector<std::byte> expected;
        std::vector<std::vector<std::byte>> messages;
        constexpr unsigned char messageCount = 8;
        constexpr std::size_t messageSize = 32768;
        messages.reserve(messageCount);
        expected.reserve(messageCount * messageSize);
        for (unsigned char index = 1; index <= messageCount; ++index)
        {
            messages.emplace_back(messageSize, static_cast<std::byte>(index));
            expected.insert(expected.end(), messages.back().begin(), messages.back().end());
        }
        // All sends run inside one strand handler, before any write completion can run.
        OnExecutor(_context, _strand, [&] -> void {
            for (auto& message : messages)
                EXPECT_TRUE(_connection->Send(std::move(message)).get().has_value());
        });
        messages.clear();

        std::vector<std::byte> received(expected.size());
        auto read = boost::asio::async_read(_peer,
                                            boost::asio::buffer(received),
                                            boost::asio::use_future);
        EXPECT_EQ(AwaitFuture(_context, std::move(read)), expected.size());
        EXPECT_EQ(received, expected);
    }

    TEST_F(ConnectionTest, ReadsMoreThanOneBufferAndClosesOnPeerEof)
    {
        Start();
        constexpr auto payloadSize = static_cast<const std::size_t>(128 * 1024);
        const std::vector<std::byte> data(payloadSize, std::byte {42});
        auto write = boost::asio::async_write(_peer,
                                              boost::asio::buffer(data),
                                              boost::asio::use_future);
        ASSERT_EQ(AwaitFuture(_context, std::move(write)), data.size());
        _peer.shutdown(Tcp::socket::shutdown_send);
        const std::weak_ptr weakConnection = _connection;
        _connection.reset();
        // Expiration requires reading through the stream to EOF and removal by the manager.
        ASSERT_TRUE(PumpUntil(_context, [&] -> bool { return weakConnection.expired(); }));
        const auto replacement = _manager.CreateConnection(Tcp::socket(_strand));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_TRUE(AwaitFuture(_context, (*replacement)->AsyncClose()).has_value());
    }

    TEST_F(ConnectionTest, PeerResetClosesAndReleasesConnection)
    {
        Start();
        _peer.set_option(boost::asio::socket_base::linger(true, 0));
        _peer.close();
        const std::weak_ptr weakConnection = _connection;
        _connection.reset();
        ASSERT_TRUE(PumpUntil(_context, [&] -> bool { return weakConnection.expired(); }));
        const auto replacement = _manager.CreateConnection(Tcp::socket(_strand));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_TRUE(AwaitFuture(_context, (*replacement)->AsyncClose()).has_value());
    }

    TEST_F(ConnectionTest, CloseBeforeStartIsIdempotentAndRejectsFurtherOperations)
    {
        EXPECT_TRUE(AwaitFuture(_context, _connection->AsyncClose()).has_value());
        EXPECT_TRUE(AwaitFuture(_context, _connection->AsyncClose()).has_value());
        const auto start = AwaitFuture(_context, _connection->AsyncStart());
        const auto send = AwaitFuture(_context, _connection->Send({std::byte {1}}));
        const auto emptySend = AwaitFuture(_context, _connection->Send({}));
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
        OnExecutor(_context, _strand, [&] -> void {
            constexpr auto pendingPayloadSize = static_cast<const std::size_t>(4 * 1024 * 1024);
            EXPECT_TRUE(_connection->Send(std::vector<std::byte>(pendingPayloadSize, std::byte {1}))
                            .get()
                            .has_value());
            EXPECT_TRUE(
                _connection->Send(std::vector<std::byte>(1024, std::byte {2})).get().has_value());
            EXPECT_TRUE(_connection->AsyncClose().get().has_value());
        });
        const std::weak_ptr weakConnection = _connection;
        _connection.reset();
        EXPECT_TRUE(PumpUntil(_context, [&] -> bool { return weakConnection.expired(); }));
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
        ASSERT_TRUE(PumpUntil(context, [&] -> bool { return weakConnection.expired(); }));
        const auto replacement = manager.CreateConnection(Tcp::socket(strand));
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
        EXPECT_TRUE(PumpUntil(context, [&] -> bool { return weakConnection.expired(); }));
    }

    TEST(ConnectionAsyncTest, FailedStartRemainsUnstartedUntilExplicitClose)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto connection = manager.CreateConnection(Tcp::socket(context));
        ASSERT_TRUE(connection.has_value());
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            const auto started = AwaitFuture(context, connection.value()->AsyncStart());
            ASSERT_FALSE(started.has_value());
            EXPECT_EQ(started.error(), ErrorCode(boost::asio::error::bad_descriptor));
        }
        const auto sent = AwaitFuture(context, connection.value()->Send({}));
        ASSERT_FALSE(sent.has_value());
        EXPECT_EQ(sent.error(), MakeErrorCode(Error::ConnectionNotStarted));
        ASSERT_TRUE(AwaitFuture(context, connection.value()->AsyncClose()).has_value());
        const auto restarted = AwaitFuture(context, connection.value()->AsyncStart());
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
            const auto result = OnExecutor(
                context,
                context.get_executor(),
                [&] -> std::expected<ConnectionManager::ConnectionPtr, ErrorCode> {
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
        EXPECT_TRUE(PumpUntil(context, [&] -> bool { return weakConnection.expired(); }));
    }
}   // namespace Flux::Test
