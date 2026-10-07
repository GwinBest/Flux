#include "gtest/gtest.h"

#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/write.hpp>
#include <chrono>
#include <cstddef>
#include <memory>
#include <stop_token>
#include <thread>
#include <vector>

#include "../Support/Async.hpp"
#include "Core/Connection.hpp"
#include "Core/ConnectionManager.hpp"
#include "Core/IoContextPool.hpp"

namespace Flux::Test
{
    namespace
    {
        using Tcp = boost::asio::ip::tcp;
        using namespace std::chrono_literals;

        class ConnectionIdleTimeoutTest : public testing::Test
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
                static constexpr auto BufferSize = 4096U;
                Tcp::acceptor listener(_context, {boost::asio::ip::address_v4::loopback(), 0});
                _peer.open(Tcp::v4());
                _peer.set_option(Tcp::socket::receive_buffer_size(BufferSize));
                _peer.connect(listener.local_endpoint());
                _peer.non_blocking(true);
                Tcp::socket socket(_strand);
                listener.accept(socket);
                socket.set_option(Tcp::socket::send_buffer_size(BufferSize));
                auto result = _manager.CreateConnection(std::move(socket), 1s);
                ASSERT_TRUE(result.has_value());
                _connection = std::move(*result);
            }

            auto TearDown() -> void override
            {
                EXPECT_TRUE(OnExecutor(_context, _strand, [&] -> Result {
                                return _manager.CloseAllConnections();
                            }).has_value());
                _context.restart();
                _context.poll();
            }

            auto Start() -> void
            {
                ASSERT_TRUE(AwaitFuture(_context, _connection->AsyncStart()).has_value());
            }

            auto RunFor(const std::chrono::milliseconds duration) -> void
            {
                // Keep processing socket completions while measuring real idle time.
                boost::asio::steady_timer timer(_context, duration);
                AwaitFuture(_context, timer.async_wait(boost::asio::use_future));
            }

            auto ExpectOpen() -> void
            {
                ASSERT_TRUE(AwaitFuture(_context, _connection->Send({})).has_value());
                char byte = 0;
                ErrorCode error;
                EXPECT_EQ(_peer.read_some(boost::asio::buffer(&byte, 1), error), 0U);
                EXPECT_TRUE(error == boost::asio::error::would_block
                            || error == boost::asio::error::try_again)
                    << error.message();
            }

            auto ExpectClosed() -> void
            {
                const auto sent = AwaitFuture(_context, _connection->Send({}));
                ASSERT_FALSE(sent.has_value());
                EXPECT_EQ(sent.error(), MakeErrorCode(Error::ConnectionClosed));
                const auto started = AwaitFuture(_context, _connection->AsyncStart());
                ASSERT_FALSE(started.has_value());
                EXPECT_EQ(started.error(), MakeErrorCode(Error::ConnectionClosed));
            }

            auto AwaitPeerClose() -> void
            {
                ErrorCode error;
                ASSERT_TRUE(PumpUntil(_context, [&] -> bool {
                    char byte = 0;
                    const auto size = _peer.read_some(boost::asio::buffer(&byte, 1), error);
                    EXPECT_EQ(size, 0U);
                    return error && error != boost::asio::error::would_block
                        && error != boost::asio::error::try_again;
                }));
                EXPECT_TRUE(error == boost::asio::error::eof
                            || error == boost::asio::error::connection_reset)
                    << error.message();
            }

            auto ReceiveActivity() -> void
            {
                constexpr char byte = 'r';
                ASSERT_EQ(boost::asio::write(_peer, boost::asio::buffer(&byte, 1)), 1U);
            }

            auto SendActivity() -> void
            {
                ASSERT_TRUE(AwaitFuture(_context, _connection->Send({std::byte {42}})).has_value());
                char byte = 0;
                ErrorCode error;
                std::size_t size = 0;
                ASSERT_TRUE(PumpUntil(_context, [&] -> bool {
                    size = _peer.read_some(boost::asio::buffer(&byte, 1), error);
                    return size != 0
                        || (error && error != boost::asio::error::would_block
                            && error != boost::asio::error::try_again);
                }));
                ASSERT_FALSE(error) << error.message();
                ASSERT_EQ(size, 1U);
                EXPECT_EQ(byte, 42);
            }
        };
    }   // namespace

    TEST_F(ConnectionIdleTimeoutTest, CustomTimeoutIsPreservedAndCanBeChangedBeforeStart)
    {
        EXPECT_EQ(_connection->GetIdleTimeout(), 1s);
        _connection->SetIdleTimeout(2s);
        EXPECT_EQ(_connection->GetIdleTimeout(), 2s);
        ASSERT_NO_FATAL_FAILURE(Start());
        RunFor(1200ms);
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, DoesNotArmTimerUntilStarted)
    {
        RunFor(1200ms);
        const auto sent = AwaitFuture(_context, _connection->Send({}));
        ASSERT_FALSE(sent.has_value());
        EXPECT_EQ(sent.error(), MakeErrorCode(Error::ConnectionNotStarted));
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, InactivityClosesPeerOnlyAfterDeadline)
    {
        const auto startedAt = std::chrono::steady_clock::now();
        ASSERT_NO_FATAL_FAILURE(Start());
        RunFor(400ms);
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        EXPECT_GE(std::chrono::steady_clock::now() - startedAt, 1s);
        ASSERT_NO_FATAL_FAILURE(ExpectClosed());
        EXPECT_TRUE(AwaitFuture(_context, _connection->AsyncClose()).has_value());
    }

    TEST_F(ConnectionIdleTimeoutTest, ReadActivityRestartsDeadlineAndCancelledWaitDoesNotClose)
    {
        ASSERT_NO_FATAL_FAILURE(Start());
        RunFor(650ms);
        ASSERT_NO_FATAL_FAILURE(ReceiveActivity());
        RunFor(650ms);
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, WriteActivityRestartsDeadlineAndCancelledWaitDoesNotClose)
    {
        ASSERT_NO_FATAL_FAILURE(Start());
        RunFor(650ms);
        ASSERT_NO_FATAL_FAILURE(SendActivity());
        RunFor(650ms);
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, RepeatedReadAndWriteActivityKeepsConnectionAlive)
    {
        ASSERT_NO_FATAL_FAILURE(Start());
        for (int index = 0; index < 4; ++index)
        {
            SCOPED_TRACE(index);
            RunFor(400ms);
            ASSERT_NO_FATAL_FAILURE(ReceiveActivity());
            ASSERT_NO_FATAL_FAILURE(SendActivity());
            ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        }
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, EmptySendDoesNotExtendDeadline)
    {
        ASSERT_NO_FATAL_FAILURE(Start());
        RunFor(650ms);
        ASSERT_TRUE(AwaitFuture(_context, _connection->Send({})).has_value());
        RunFor(550ms);
        ASSERT_NO_FATAL_FAILURE(ExpectClosed());
        AwaitPeerClose();
    }

    TEST_F(ConnectionIdleTimeoutTest, RejectedRepeatedStartDoesNotExtendDeadline)
    {
        ASSERT_NO_FATAL_FAILURE(Start());
        RunFor(650ms);
        const auto repeated = AwaitFuture(_context, _connection->AsyncStart());
        ASSERT_FALSE(repeated.has_value());
        EXPECT_EQ(repeated.error(), MakeErrorCode(Error::ConnectionAlreadyStarted));
        RunFor(550ms);
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, UpdatedTimeoutIsUsedOnNextRead)
    {
        _connection->SetIdleTimeout(5s);
        ASSERT_NO_FATAL_FAILURE(Start());
        _connection->SetIdleTimeout(1s);
        ASSERT_NO_FATAL_FAILURE(ReceiveActivity());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, UpdatedTimeoutIsUsedOnNextWrite)
    {
        _connection->SetIdleTimeout(5s);
        ASSERT_NO_FATAL_FAILURE(Start());
        _connection->SetIdleTimeout(1s);
        ASSERT_NO_FATAL_FAILURE(SendActivity());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, ShorteningActiveTimeoutRearmsWithoutTraffic)
    {
        _connection->SetIdleTimeout(5s);
        ASSERT_NO_FATAL_FAILURE(Start());
        _connection->SetIdleTimeout(1s);
        EXPECT_EQ(_connection->GetIdleTimeout(), 1s);
        RunFor(1200ms);
        ASSERT_NO_FATAL_FAILURE(ExpectClosed());
        AwaitPeerClose();
    }

    TEST_F(ConnectionIdleTimeoutTest, LengtheningActiveTimeoutRearmsWithoutTraffic)
    {
        ASSERT_NO_FATAL_FAILURE(Start());
        RunFor(650ms);
        _connection->SetIdleTimeout(2s);
        EXPECT_EQ(_connection->GetIdleTimeout(), 2s);
        RunFor(650ms);
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, SettingSameTimeoutRestartsCountdownWithoutTraffic)
    {
        ASSERT_NO_FATAL_FAILURE(Start());
        RunFor(650ms);
        _connection->SetIdleTimeout(1s);
        RunFor(650ms);
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, ZeroTimeoutClosesActiveConnectionWithoutTraffic)
    {
        _connection->SetIdleTimeout(5s);
        ASSERT_NO_FATAL_FAILURE(Start());
        _connection->SetIdleTimeout(0s);
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, NegativeTimeoutClosesActiveConnectionWithoutTraffic)
    {
        _connection->SetIdleTimeout(5s);
        ASSERT_NO_FATAL_FAILURE(Start());
        _connection->SetIdleTimeout(-1s);
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, PendingTimeoutUpdatesKeepTheLatestDeadline)
    {
        _connection->SetIdleTimeout(5s);
        ASSERT_NO_FATAL_FAILURE(Start());
        _connection->SetIdleTimeout(0s);
        _connection->SetIdleTimeout(2s);
        EXPECT_EQ(_connection->GetIdleTimeout(), 2s);
        RunFor(1200ms);
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, RearmingIgnoresAnAlreadyQueuedExpiredWait)
    {
        ASSERT_NO_FATAL_FAILURE(Start());
        const auto work = boost::asio::make_work_guard(_context);
        const std::jthread worker([&](const std::stop_token& stopToken) -> void {
            const std::stop_callback stop(stopToken, [&] -> void { _context.stop(); });
            _context.run();
        });
        OnExecutor(_context, _strand, [&] -> void {
            // The other runner queues the expired idle wait behind this strand handler.
            boost::asio::steady_timer gate(_context, 1200ms);
            auto ready = gate.async_wait(boost::asio::use_future);
            ASSERT_EQ(ready.wait_for(3s), std::future_status::ready);
            ready.get();
            ASSERT_TRUE(_connection->Send({}).get().has_value());
            _connection->SetIdleTimeout(2s);
        });
        RunFor(400ms);
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, UpdatingClosedConnectionDoesNotArmTimerOrRetainOwnership)
    {
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_TRUE(AwaitFuture(_context, _connection->AsyncClose()).has_value());
        _connection->SetIdleTimeout(5s);
        EXPECT_EQ(_connection->GetIdleTimeout(), 5s);
        const std::weak_ptr weakConnection = _connection;
        _connection.reset();
        ASSERT_TRUE(PumpUntil(_context, [&] -> bool { return weakConnection.expired(); }));
        _context.restart();
        _context.poll();
        EXPECT_TRUE(_context.stopped());
    }

    TEST(ConnectionIdleTimeoutAsyncTest, ControlThreadTimeoutChangeRearmsWorkerTimerWithoutTraffic)
    {
        IoContextPool pool(2);
        ASSERT_TRUE(pool.Run().has_value());
        boost::asio::io_context context;
        Tcp::acceptor listener(context, {boost::asio::ip::address_v4::loopback(), 0});
        Tcp::socket peer(context);
        peer.connect(listener.local_endpoint());
        Tcp::socket socket(pool.GetStrand());
        listener.accept(socket);
        const ConnectionManager manager(1);
        const auto connection = manager.CreateAndStartConnection(std::move(socket), 5s);
        ASSERT_TRUE(connection.has_value());
        connection.value()->SetIdleTimeout(1s);
        EXPECT_EQ((*connection)->GetIdleTimeout(), 1s);

        peer.non_blocking(true);
        ErrorCode error;
        const auto closed = PumpUntil(context, [&] -> bool {
            char byte = 0;
            const auto size = peer.read_some(boost::asio::buffer(&byte, 1), error);
            EXPECT_EQ(size, 0U);
            return error && error != boost::asio::error::would_block
                && error != boost::asio::error::try_again;
        });
        EXPECT_TRUE(closed);
        if (closed)
        {
            EXPECT_TRUE(error == boost::asio::error::eof
                        || error == boost::asio::error::connection_reset)
                << error.message();
        }
        EXPECT_TRUE(manager.CloseAllConnections().has_value());
        EXPECT_TRUE(pool.Stop().has_value());
    }

    TEST_F(ConnectionIdleTimeoutTest, ZeroTimeoutExpiresOnStart)
    {
        _connection->SetIdleTimeout(0s);
        EXPECT_EQ(_connection->GetIdleTimeout(), 0s);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, NegativeTimeoutExpiresOnStart)
    {
        _connection->SetIdleTimeout(-1s);
        EXPECT_EQ(_connection->GetIdleTimeout(), -1s);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        ExpectClosed();
    }

    TEST_F(ConnectionIdleTimeoutTest, TimeoutReleasesPendingReadOwnershipAndManagerCapacity)
    {
        ASSERT_NO_FATAL_FAILURE(Start());
        const std::weak_ptr weakConnection = _connection;
        _connection.reset();
        ASSERT_TRUE(PumpUntil(_context, [&] -> bool { return weakConnection.expired(); }));
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        const auto replacement = _manager.CreateConnection(Tcp::socket(_strand));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_TRUE(AwaitFuture(_context, (*replacement)->AsyncClose()).has_value());
        _context.restart();
        _context.poll();
        EXPECT_TRUE(_context.stopped());
    }

    TEST_F(ConnectionIdleTimeoutTest, ExplicitCloseCancelsLongTimerAndReleasesOwnershipPromptly)
    {
        _connection->SetIdleTimeout(1h);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_TRUE(AwaitFuture(_context, _connection->AsyncClose()).has_value());
        const std::weak_ptr weakConnection = _connection;
        _connection.reset();
        ASSERT_TRUE(PumpUntil(_context, [&] -> bool { return weakConnection.expired(); }));
        ASSERT_NO_FATAL_FAILURE(AwaitPeerClose());
        _context.restart();
        _context.poll();
        EXPECT_TRUE(_context.stopped());
    }

    TEST_F(ConnectionIdleTimeoutTest, TimeoutCancelsBlockedWriteAndQueuedPayloads)
    {
        ASSERT_NO_FATAL_FAILURE(Start());
        OnExecutor(_context, _strand, [&] -> void {
            constexpr auto payloadSize = static_cast<std::size_t>(4 * 1024 * 1024);
            ASSERT_TRUE(_connection->Send(std::vector<std::byte>(payloadSize, std::byte {1}))
                            .get()
                            .has_value());
            ASSERT_TRUE(_connection->Send(std::vector<std::byte>(payloadSize, std::byte {2}))
                            .get()
                            .has_value());
        });
        const std::weak_ptr weakConnection = _connection;
        _connection.reset();
        ASSERT_TRUE(PumpUntil(_context, [&] -> bool { return weakConnection.expired(); }));
        const auto replacement = _manager.CreateConnection(Tcp::socket(_strand));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_TRUE(AwaitFuture(_context, (*replacement)->AsyncClose()).has_value());
    }
}   // namespace Flux::Test
