#include "gtest/gtest.h"

#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/write.hpp>
#include <chrono>
#include <cstddef>
#include <memory>
#include <vector>

#include "../Support/Async.hpp"
#include "Core/Connection.hpp"
#include "Core/ConnectionManager.hpp"

namespace Flux::Test
{
    namespace
    {
        using Tcp = boost::asio::ip::tcp;
        using namespace std::chrono_literals;

        class ConnectionRequestSizeTest : public testing::Test
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
                _peer.set_option(Tcp::no_delay(true));
                _peer.non_blocking(true);
                Tcp::socket socket(_strand);
                listener.accept(socket);
                auto result = _manager.CreateConnection(std::move(socket));
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

            auto ReceiveBytes(const std::size_t size) -> void
            {
                const std::vector<std::byte> data(size, std::byte {42});
                auto write = boost::asio::async_write(_peer,
                                                      boost::asio::buffer(data),
                                                      boost::asio::use_future);
                ASSERT_EQ(AwaitFuture(_context, std::move(write)), size);

                // Write completion only confirms transmission. Keep driving the receiver
                // before checking that it is still open or sending the next fragment.
                boost::asio::steady_timer settle(_context, 50ms);
                AwaitFuture(_context, settle.async_wait(boost::asio::use_future));
            }

            auto ExpectOpen() -> void
            {
                ASSERT_TRUE(AwaitFuture(_context, _connection->Send({})).has_value());
                std::byte byte {};
                ErrorCode error;
                EXPECT_EQ(_peer.read_some(boost::asio::buffer(&byte, 1), error), 0U);
                EXPECT_TRUE(error == boost::asio::error::would_block
                            || error == boost::asio::error::try_again)
                    << error.message();
            }

            auto ExpectClosed() -> void
            {
                ErrorCode error;
                ASSERT_TRUE(PumpUntil(_context, [&] -> bool {
                    std::byte byte {};
                    EXPECT_EQ(_peer.read_some(boost::asio::buffer(&byte, 1), error), 0U);
                    return error && error != boost::asio::error::would_block
                        && error != boost::asio::error::try_again;
                }));
                ASSERT_TRUE(error == boost::asio::error::eof
                            || error == boost::asio::error::connection_reset)
                    << error.message();

                const auto sent = AwaitFuture(_context, _connection->Send({}));
                ASSERT_FALSE(sent.has_value());
                EXPECT_EQ(sent.error(), MakeErrorCode(Error::ConnectionClosed));
                const auto started = AwaitFuture(_context, _connection->AsyncStart());
                ASSERT_FALSE(started.has_value());
                EXPECT_EQ(started.error(), MakeErrorCode(Error::ConnectionClosed));
            }
        };

        class ConnectionRequestSizeBoundaryTest : public ConnectionRequestSizeTest,
                                                  public testing::WithParamInterface<std::size_t>
        {};
    }   // namespace

    TEST_F(ConnectionRequestSizeTest, DefaultLimitAccepts32KiBAndRejectsOneMoreByte)
    {
        constexpr auto defaultLimit = static_cast<std::size_t>(32 * 1024);
        EXPECT_EQ(_connection->GetMaxRequestSize(), defaultLimit);
        ASSERT_NO_FATAL_FAILURE(Start());

        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(defaultLimit - 1));
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(1));
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(1));
        ExpectClosed();
    }

    TEST_P(ConnectionRequestSizeBoundaryTest, CustomLimitAcceptsExactSizeAndRejectsOneMoreByte)
    {
        const auto limit = GetParam();
        _connection->SetMaxRequestSize(limit);
        EXPECT_EQ(_connection->GetMaxRequestSize(), limit);
        ASSERT_NO_FATAL_FAILURE(Start());

        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(limit));
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(1));
        ExpectClosed();
    }

    INSTANTIATE_TEST_SUITE_P(RequestLimits,
                             ConnectionRequestSizeBoundaryTest,
                             testing::Values(1U, 17U, 8192U, 8193U, 65537U));

    TEST_F(ConnectionRequestSizeTest, ShortReadsCountOnlyReceivedBytesAndAccumulate)
    {
        constexpr std::size_t limit = 8;
        _connection->SetMaxRequestSize(limit);
        ASSERT_NO_FATAL_FAILURE(Start());

        for (std::size_t received = 0; received < limit; ++received)
        {
            SCOPED_TRACE(received);
            ASSERT_NO_FATAL_FAILURE(ReceiveBytes(1));
            ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        }
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(1));
        ExpectClosed();
    }

    TEST_F(ConnectionRequestSizeTest, ZeroLimitKeepsIdleConnectionOpenButRejectsFirstByte)
    {
        _connection->SetMaxRequestSize(0);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());

        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(1));
        ExpectClosed();
    }

    TEST_F(ConnectionRequestSizeTest, IncreasingActiveLimitPreservesAlreadyReceivedBytes)
    {
        constexpr std::size_t initialLimit = 3;
        constexpr std::size_t raisedLimit = initialLimit * 2;
        _connection->SetMaxRequestSize(initialLimit);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(initialLimit));
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());

        _connection->SetMaxRequestSize(raisedLimit);
        EXPECT_EQ(_connection->GetMaxRequestSize(), raisedLimit);
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(raisedLimit - initialLimit));
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(1));
        ExpectClosed();
    }

    TEST_F(ConnectionRequestSizeTest, DecreasingActiveLimitUsesAlreadyReceivedBytes)
    {
        constexpr std::size_t initialLimit = 8;
        constexpr std::size_t reducedLimit = 4;
        _connection->SetMaxRequestSize(initialLimit);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(reducedLimit - 1));
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());

        _connection->SetMaxRequestSize(reducedLimit);
        EXPECT_EQ(_connection->GetMaxRequestSize(), reducedLimit);
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(1));
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(1));
        ExpectClosed();
    }

    TEST_F(ConnectionRequestSizeTest, LimitReducedBelowBufferedSizeClosesOnNextRead)
    {
        constexpr std::size_t initialLimit = 8;
        constexpr std::size_t bufferedSize = 3;
        _connection->SetMaxRequestSize(initialLimit);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(bufferedSize));
        ASSERT_NO_FATAL_FAILURE(ExpectOpen());

        _connection->SetMaxRequestSize(bufferedSize - 1);
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(1));
        ExpectClosed();
    }

    TEST_F(ConnectionRequestSizeTest, OversizedInputClosesPeerAndReleasesManagerCapacity)
    {
        constexpr std::size_t limit = 8;
        _connection->SetMaxRequestSize(limit);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(ReceiveBytes(limit + 1));
        ASSERT_NO_FATAL_FAILURE(ExpectClosed());

        const std::weak_ptr weakConnection = _connection;
        _connection.reset();
        ASSERT_TRUE(PumpUntil(_context, [&] -> bool { return weakConnection.expired(); }));

        const auto replacement = _manager.CreateConnection(Tcp::socket(_strand));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_TRUE(AwaitFuture(_context, (*replacement)->AsyncClose()).has_value());
    }
}   // namespace Flux::Test
