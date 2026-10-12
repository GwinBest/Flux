#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <barrier>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>
#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <stop_token>
#include <thread>
#include <vector>

#include "../Support/AfterExecutionExecutor.hpp"
#include "../Support/Async.hpp"
#include "Core/Connection.hpp"
#include "Core/ConnectionManager.hpp"

namespace Flux::Test
{
    namespace
    {
        using Tcp = boost::asio::ip::tcp;
        using namespace std::chrono_literals;

        auto IsError(const Result& result, const Error expected) -> testing::AssertionResult
        {
            if (result.has_value())
                return testing::AssertionFailure()
                    << "Expected " << MakeErrorCode(expected).message()
                    << ", but the operation succeeded";
            if (result.error() != MakeErrorCode(expected))
                return testing::AssertionFailure()
                    << "Expected " << MakeErrorCode(expected).message() << ", got "
                    << result.error().message();
            return testing::AssertionSuccess();
        }

        class ConnectionSendQueueTest : public testing::Test
        {
        protected:
            boost::asio::io_context _context;
            boost::asio::strand<boost::asio::io_context::executor_type>
                _strand = boost::asio::make_strand(_context);
            ConnectionManager _manager {1};
            Tcp::socket _peer {_context};
            ConnectionManager::ConnectionPtr _connection;
            // Keep async read buffers alive even when a test's deadline expires.
            std::vector<std::byte> _received;

            virtual auto GetExecutor() -> boost::asio::any_io_executor { return _strand; }

            auto SetUp() -> void override
            {
                constexpr int receiveBufferSize = 16384;
                constexpr int sendBufferSize = 1024;
                Tcp::acceptor listener(_context, {boost::asio::ip::address_v4::loopback(), 0});
                _peer.open(Tcp::v4());
                _peer.set_option(Tcp::socket::receive_buffer_size(receiveBufferSize));
                _peer.connect(listener.local_endpoint());
                Tcp::socket socket(GetExecutor());
                listener.accept(socket);
                socket.set_option(Tcp::socket::send_buffer_size(sendBufferSize));
                socket.set_option(Tcp::no_delay(true));
                auto result = _manager.CreateConnection(std::move(socket));
                ASSERT_TRUE(result.has_value());
                _connection = std::move(*result);
            }

            auto TearDown() -> void override
            {
                EXPECT_TRUE(OnExecutor(_context, _strand, [&] -> Result {
                                return _manager.CloseAllConnections();
                            }).has_value());
                ErrorCode error;
                EXPECT_FALSE(_peer.close(error)) << error.message();
                _context.restart();
                _context.poll();
            }

            auto Start() -> void
            {
                ASSERT_TRUE(AwaitFuture(_context, _connection->AsyncStart()).has_value());
            }

            auto ReadBytes(Tcp::socket& peer, const std::size_t size) -> void
            {
                _received.resize(size);
                auto read = boost::asio::async_read(peer,
                                                    boost::asio::buffer(_received),
                                                    boost::asio::use_future);
                ASSERT_EQ(AwaitFuture(_context, std::move(read)), size);
            }

            auto ExpectReceived(const std::vector<std::byte>& expected) -> void
            {
                ASSERT_NO_FATAL_FAILURE(ReadBytes(_peer, expected.size()));
                EXPECT_EQ(_received, expected);
            }

            auto SendAfterDrain(const std::vector<std::byte>& data) -> void
            {
                // Peer receipt can precede the sender's completion handler. Retry only
                // QueueFull until that handler has released the queue's capacity.
                auto sent = _connection->Send(std::vector(data));
                Result result;
                ASSERT_TRUE(PumpUntil(_context, [&] -> bool {
                    if (sent.wait_for(0s) != std::future_status::ready)
                        return false;
                    result = sent.get();
                    if (result || result.error() != MakeErrorCode(Error::SendQueueFull))
                        return true;
                    sent = _connection->Send(std::vector(data));
                    return false;
                }));
                ASSERT_TRUE(result.has_value()) << result.error().message();
            }
        };

        class ConnectionSendQueueBoundaryTest : public ConnectionSendQueueTest,
                                                public testing::WithParamInterface<std::size_t>
        {};

        class ConnectionSendQueueCompletionTest : public ConnectionSendQueueTest
        {
        protected:
            std::shared_ptr<std::function<void()>>
                _afterNext = std::make_shared<std::function<void()>>();

            auto GetExecutor() -> boost::asio::any_io_executor override
            {
                return AfterExecutionExecutor(_strand, _afterNext);
            }

            auto TearDown() -> void override
            {
                // An early assertion may leave an observer referring to test locals.
                *_afterNext = {};
                ConnectionSendQueueTest::TearDown();
            }
        };
    }   // namespace

    TEST_F(ConnectionSendQueueTest, DefaultLimitAccepts32KiBAndRejectsOneMoreByte)
    {
        constexpr std::size_t defaultLimit = std::size_t {32} * 1024;
        EXPECT_EQ(_connection->GetMaxSendQueue(), defaultLimit);
        ASSERT_NO_FATAL_FAILURE(Start());
        const std::vector<std::byte> payload(defaultLimit, std::byte {42});

        // Write completions cannot run between these sends on the same strand.
        ASSERT_NO_FATAL_FAILURE(OnExecutor(_context, _strand, [&] -> void {
            ASSERT_TRUE(_connection->Send(std::vector(payload)).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
            EXPECT_TRUE(_connection->Send({}).get().has_value());
        }));
        ExpectReceived(payload);
    }

    TEST_P(ConnectionSendQueueBoundaryTest, RejectsOversizedPayloadWithoutConsumingCapacity)
    {
        const auto limit = GetParam();
        _connection->SetMaxSendQueue(limit);
        EXPECT_EQ(_connection->GetMaxSendQueue(), limit);
        ASSERT_NO_FATAL_FAILURE(Start());
        const std::vector<std::byte> payload(limit, std::byte {42});

        ASSERT_NO_FATAL_FAILURE(OnExecutor(_context, _strand, [&] -> void {
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                EXPECT_TRUE(IsError(
                    _connection->Send(std::vector<std::byte>(limit + 1, std::byte {99})).get(),
                    Error::SendQueueFull));
            }
            ASSERT_TRUE(_connection->Send(std::vector(payload)).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
        }));
        ExpectReceived(payload);
    }

    INSTANTIATE_TEST_SUITE_P(SendLimits,
                             ConnectionSendQueueBoundaryTest,
                             testing::Values(1U, 17U, 8192U, 8193U, 65537U));

    TEST_F(ConnectionSendQueueTest, CountsBytesAcrossAllQueuedMessagesAndPreservesAcceptedOrder)
    {
        constexpr std::size_t limit = 8;
        _connection->SetMaxSendQueue(limit);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(OnExecutor(_context, _strand, [&] -> void {
            ASSERT_TRUE(
                _connection->Send(std::vector<std::byte>(3, std::byte {1})).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send(std::vector<std::byte>(6, std::byte {99})).get(),
                                Error::SendQueueFull));
            ASSERT_TRUE(
                _connection->Send(std::vector<std::byte>(4, std::byte {2})).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send(std::vector<std::byte>(2, std::byte {99})).get(),
                                Error::SendQueueFull));
            ASSERT_TRUE(_connection->Send({std::byte {3}}).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
            EXPECT_TRUE(_connection->Send({}).get().has_value());
        }));
        ASSERT_NO_FATAL_FAILURE(ExpectReceived({
            std::byte {1},
            std::byte {1},
            std::byte {1},
            std::byte {2},
            std::byte {2},
            std::byte {2},
            std::byte {2},
            std::byte {3},
        }));
        // A following full message also detects rejected bytes left in the stream.
        const std::vector<std::byte> next(limit, std::byte {42});
        ASSERT_NO_FATAL_FAILURE(SendAfterDrain(next));
        ExpectReceived(next);
    }

    TEST_F(ConnectionSendQueueTest, ZeroLimitAllowsEmptySendsAndCanBeRaised)
    {
        constexpr std::byte payloadByte {42};
        _connection->SetMaxSendQueue(0);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(OnExecutor(_context, _strand, [&] -> void {
            EXPECT_TRUE(_connection->Send({}).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
            _connection->SetMaxSendQueue(1);
            ASSERT_TRUE(_connection->Send({payloadByte}).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
        }));
        ExpectReceived({payloadByte});
    }

    TEST_F(ConnectionSendQueueTest, LifecycleErrorsTakePriorityOverZeroLimit)
    {
        _connection->SetMaxSendQueue(0);
        for (const std::size_t size : {0U, 1U, 65537U})
        {
            EXPECT_TRUE(
                IsError(AwaitFuture(_context, _connection->Send(std::vector<std::byte>(size))),
                        Error::ConnectionNotStarted));
        }
        _connection->SetMaxSendQueue(1);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_TRUE(AwaitFuture(_context, _connection->Send({std::byte {42}})).has_value());
        ASSERT_NO_FATAL_FAILURE(ExpectReceived({std::byte {42}}));
        ASSERT_TRUE(AwaitFuture(_context, _connection->AsyncClose()).has_value());
        _connection->SetMaxSendQueue(0);
        for (const std::size_t size : {0U, 1U, 65537U})
        {
            EXPECT_TRUE(
                IsError(AwaitFuture(_context, _connection->Send(std::vector<std::byte>(size))),
                        Error::ConnectionClosed));
        }
    }

    TEST_F(ConnectionSendQueueTest, ConfigurationPreservesSizeTValuesAndIsIndependentOfRequestLimit)
    {
        constexpr std::byte payloadByte {42};
        const auto requestLimit = _connection->GetMaxRequestSize();
        for (const auto limit :
             std::array<std::size_t, 4> {0, 1, 65537, std::numeric_limits<std::size_t>::max()})
        {
            _connection->SetMaxSendQueue(limit);
            EXPECT_EQ(_connection->GetMaxSendQueue(), limit);
            EXPECT_EQ(_connection->GetMaxRequestSize(), requestLimit);
        }
        _connection->SetMaxRequestSize(0);
        EXPECT_EQ(_connection->GetMaxSendQueue(), std::numeric_limits<std::size_t>::max());
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_TRUE(AwaitFuture(_context, _connection->Send({payloadByte})).has_value());
        ExpectReceived({payloadByte});
    }

    TEST_F(ConnectionSendQueueTest, IncreasingActiveLimitKeepsAlreadyQueuedBytesAccounted)
    {
        _connection->SetMaxSendQueue(3);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(OnExecutor(_context, _strand, [&] -> void {
            ASSERT_TRUE(
                _connection->Send(std::vector<std::byte>(3, std::byte {1})).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
            _connection->SetMaxSendQueue(6);
            EXPECT_TRUE(IsError(_connection->Send(std::vector<std::byte>(4, std::byte {99})).get(),
                                Error::SendQueueFull));
            ASSERT_TRUE(
                _connection->Send(std::vector<std::byte>(3, std::byte {2})).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
        }));
        ExpectReceived({
            std::byte {1},
            std::byte {1},
            std::byte {1},
            std::byte {2},
            std::byte {2},
            std::byte {2},
        });
    }

    TEST_F(ConnectionSendQueueTest, DecreasingActiveLimitAllowsOnlyTheRemainingCapacity)
    {
        constexpr std::size_t initialLimit = 8;
        _connection->SetMaxSendQueue(initialLimit);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(OnExecutor(_context, _strand, [&] -> void {
            ASSERT_TRUE(
                _connection->Send(std::vector<std::byte>(3, std::byte {1})).get().has_value());
            _connection->SetMaxSendQueue(4);
            EXPECT_TRUE(IsError(_connection->Send(std::vector<std::byte>(2, std::byte {99})).get(),
                                Error::SendQueueFull));
            ASSERT_TRUE(_connection->Send({std::byte {2}}).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
        }));
        ExpectReceived({std::byte {1}, std::byte {1}, std::byte {1}, std::byte {2}});
    }

    TEST_F(ConnectionSendQueueTest, LoweringLimitBelowPendingSizePreservesDataAndRecoversAfterDrain)
    {
        const std::vector<std::byte> payload(6, std::byte {1});
        _connection->SetMaxSendQueue(payload.size());
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(OnExecutor(_context, _strand, [&] -> void {
            ASSERT_TRUE(_connection->Send(std::vector(payload)).get().has_value());
            _connection->SetMaxSendQueue(3);
            EXPECT_TRUE(_connection->Send({}).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
        }));
        ASSERT_NO_FATAL_FAILURE(ExpectReceived(payload));
        const std::vector<std::byte> next(3, std::byte {2});
        ASSERT_NO_FATAL_FAILURE(SendAfterDrain(next));
        ExpectReceived(next);
    }

    TEST_F(ConnectionSendQueueTest, ZeroingActiveLimitLetsQueuedDataDrainWithoutAcceptingMore)
    {
        const std::vector<std::byte> payload(3, std::byte {42});
        _connection->SetMaxSendQueue(payload.size());
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(OnExecutor(_context, _strand, [&] -> void {
            ASSERT_TRUE(_connection->Send(std::vector(payload)).get().has_value());
            _connection->SetMaxSendQueue(0);
            EXPECT_TRUE(_connection->Send({}).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
        }));
        ASSERT_NO_FATAL_FAILURE(ExpectReceived(payload));
        EXPECT_TRUE(IsError(AwaitFuture(_context, _connection->Send({std::byte {99}})),
                            Error::SendQueueFull));
        _connection->SetMaxSendQueue(payload.size());
        ASSERT_NO_FATAL_FAILURE(SendAfterDrain(payload));
        ExpectReceived(payload);
    }

    TEST_F(ConnectionSendQueueTest, PendingSendsUseTheLimitAtExecutionTime)
    {
        ASSERT_NO_FATAL_FAILURE(Start());
        const std::vector<std::byte> payload(4, std::byte {42});
        _connection->SetMaxSendQueue(payload.size());
        auto rejected = _connection->Send(std::vector(payload));
        EXPECT_EQ(rejected.wait_for(0s), std::future_status::timeout);
        // No io_context runner is active, so the control thread updates the
        // limit before the queued send's admission check can execute.
        std::jthread([&] -> void { _connection->SetMaxSendQueue(payload.size() - 1); }).join();
        EXPECT_TRUE(IsError(AwaitFuture(_context, std::move(rejected)), Error::SendQueueFull));

        auto accepted = _connection->Send(std::vector(payload));
        EXPECT_EQ(accepted.wait_for(0s), std::future_status::timeout);
        std::jthread([&] -> void { _connection->SetMaxSendQueue(payload.size()); }).join();
        ASSERT_TRUE(AwaitFuture(_context, std::move(accepted)).has_value());
        ExpectReceived(payload);
    }

    TEST_F(ConnectionSendQueueTest, RepeatedFullQueueDrainsRestoreAllCapacity)
    {
        constexpr std::size_t limit = 17;
        _connection->SetMaxSendQueue(limit);
        ASSERT_NO_FATAL_FAILURE(Start());
        for (unsigned char round = 1; round <= 4; ++round)
        {
            SCOPED_TRACE(round);
            const std::vector<std::byte> payload(limit, static_cast<std::byte>(round));
            ASSERT_NO_FATAL_FAILURE(SendAfterDrain(payload));
            ASSERT_NO_FATAL_FAILURE(ExpectReceived(payload));
        }
    }

    TEST_F(ConnectionSendQueueCompletionTest,
           CompletedMessageReleasesCapacityBeforeNextWriteCompletes)
    {
        const std::vector<std::byte> first(1, std::byte {1});
        const std::vector<std::byte> large(65537, std::byte {2});
        const std::vector<std::byte> tail(first.size(), std::byte {3});
        bool observedCompletion = false;
        _connection->SetMaxSendQueue(first.size() + large.size());
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(OnExecutor(_context, _strand, [&] -> void {
            ASSERT_TRUE(_connection->Send(std::vector(first)).get().has_value());
            ASSERT_TRUE(_connection->Send(std::vector(large)).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
            // The next connection handler completes the first one-byte write.
            // Observe after it starts the second write, before another completion
            // can run, even if the OS has already transmitted the entire buffer.
            *_afterNext = [&] -> void {
                observedCompletion = true;
                EXPECT_TRUE(IsError(
                    _connection->Send(std::vector<std::byte>(first.size() + 1, std::byte {99}))
                        .get(),
                    Error::SendQueueFull));
                ASSERT_TRUE(_connection->Send(std::vector(tail)).get().has_value());
                EXPECT_TRUE(
                    IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
            };
        }));
        ASSERT_TRUE(PumpUntil(_context, [&] -> bool { return observedCompletion; }));
        ASSERT_FALSE(HasFatalFailure());
        auto expected = first;
        expected.insert(expected.end(), large.begin(), large.end());
        expected.insert(expected.end(), tail.begin(), tail.end());
        ASSERT_NO_FATAL_FAILURE(ExpectReceived(expected));

        const std::vector<std::byte> refill(first.size() + large.size(), std::byte {4});
        ASSERT_NO_FATAL_FAILURE(SendAfterDrain(refill));
        ExpectReceived(refill);
    }

    TEST_F(ConnectionSendQueueTest, ConcurrentCallersCannotOverfillQueueOnMultipleRunners)
    {
        constexpr std::size_t callerCount = 8;
        constexpr std::size_t sendsPerCaller = 4;
        constexpr std::size_t capacity = callerCount * 2;
        constexpr std::size_t runnerCount = 2;
        _connection->SetMaxSendQueue(capacity);
        ASSERT_NO_FATAL_FAILURE(Start());
        std::array<std::future<Result>, callerCount * sendsPerCaller> sends;
        std::barrier start(callerCount);
        {
            std::vector<std::jthread> callers;
            callers.reserve(callerCount);
            for (std::size_t caller = 0; caller < callerCount; ++caller)
            {
                callers.emplace_back([&, caller] -> void {
                    start.arrive_and_wait();
                    for (std::size_t send = 0; send < sendsPerCaller; ++send)
                    {
                        const auto index = (caller * sendsPerCaller) + send;
                        sends.at(index) = _connection->Send({static_cast<std::byte>(index)});
                    }
                });
            }
        }
        // Queue all admissions before running any write completion. The strand
        // serializes these requests even when several io_context threads run.
        for (const auto& sent : sends) EXPECT_EQ(sent.wait_for(0s), std::future_status::timeout);
        std::vector<std::jthread> runners;
        runners.reserve(runnerCount);
        for (std::size_t runner = 0; runner < runnerCount; ++runner)
        {
            runners.emplace_back([&](const std::stop_token& stopToken) -> void {
                const std::stop_callback stop(stopToken, [&] -> void { _context.stop(); });
                _context.run();
            });
        }

        std::vector<std::byte> accepted;
        for (std::size_t index = 0; index < sends.size(); ++index)
        {
            ASSERT_EQ(sends.at(index).wait_for(3s), std::future_status::ready);
            if (const auto result = sends.at(index).get(); result)
                accepted.push_back(static_cast<std::byte>(index));
            else
                EXPECT_TRUE(IsError(result, Error::SendQueueFull));
        }
        ASSERT_EQ(accepted.size(), capacity);
        ASSERT_NO_FATAL_FAILURE(ReadBytes(_peer, accepted.size()));
        std::ranges::sort(_received);
        EXPECT_EQ(_received, accepted);
    }

    TEST_F(ConnectionSendQueueTest, ConnectionsHaveIndependentLimitsAndPendingBytes)
    {
        const ConnectionManager secondManager(1);
        Tcp::acceptor listener(_context, {boost::asio::ip::address_v4::loopback(), 0});
        Tcp::socket secondPeer(_context);
        secondPeer.connect(listener.local_endpoint());
        Tcp::socket socket(_strand);
        listener.accept(socket);
        const auto second = secondManager.CreateConnection(std::move(socket));
        ASSERT_TRUE(second.has_value());
        _connection->SetMaxSendQueue(2);
        EXPECT_EQ((*second)->GetMaxSendQueue(), 32U * 1024U);
        second.value()->SetMaxSendQueue(3);
        EXPECT_EQ(_connection->GetMaxSendQueue(), 2U);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_TRUE(AwaitFuture(_context, (*second)->AsyncStart()).has_value());
        const std::vector<std::byte> firstPayload(2, std::byte {1});
        const std::vector<std::byte> secondPayload(3, std::byte {2});
        ASSERT_NO_FATAL_FAILURE(OnExecutor(_context, _strand, [&] -> void {
            ASSERT_TRUE(_connection->Send(std::vector(firstPayload)).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
            ASSERT_TRUE((*second)->Send(std::vector(secondPayload)).get().has_value());
            EXPECT_TRUE(IsError((*second)->Send({std::byte {99}}).get(), Error::SendQueueFull));
        }));
        ASSERT_NO_FATAL_FAILURE(ExpectReceived(firstPayload));
        ASSERT_NO_FATAL_FAILURE(ReadBytes(secondPeer, secondPayload.size()));
        EXPECT_EQ(_received, secondPayload);
        EXPECT_TRUE(AwaitFuture(_context, (*second)->AsyncClose()).has_value());
    }

    TEST_F(ConnectionSendQueueTest, ClosingFullQueueReleasesConnectionWhileSendFuturesRemainAlive)
    {
        constexpr std::size_t limit = std::size_t {1024} * 1024;
        _connection->SetMaxSendQueue(limit);
        ASSERT_NO_FATAL_FAILURE(Start());
        std::future<Result> accepted;
        std::future<Result> rejected;
        ASSERT_NO_FATAL_FAILURE(OnExecutor(_context, _strand, [&] -> void {
            accepted = _connection->Send(std::vector<std::byte>(limit, std::byte {1}));
            rejected = _connection->Send({std::byte {99}});
            EXPECT_EQ(accepted.wait_for(0s), std::future_status::ready);
            EXPECT_EQ(rejected.wait_for(0s), std::future_status::ready);
            ASSERT_TRUE(_connection->AsyncClose().get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {2}}).get(), Error::ConnectionClosed));
        }));
        const std::weak_ptr weakConnection = _connection;
        _connection.reset();
        ASSERT_TRUE(PumpUntil(_context, [&] -> bool { return weakConnection.expired(); }));
        EXPECT_TRUE(accepted.get().has_value());
        EXPECT_TRUE(IsError(rejected.get(), Error::SendQueueFull));
        const auto replacement = _manager.CreateConnection(Tcp::socket(_strand));
        EXPECT_TRUE(replacement.has_value());
    }

    TEST_F(ConnectionSendQueueTest, PeerResetClosesFullQueueAndReleasesManagerCapacity)
    {
        constexpr std::size_t limit = std::size_t {1024} * 1024;
        _connection->SetMaxSendQueue(limit);
        ASSERT_NO_FATAL_FAILURE(Start());
        ASSERT_NO_FATAL_FAILURE(OnExecutor(_context, _strand, [&] -> void {
            ASSERT_TRUE(
                _connection->Send(std::vector<std::byte>(limit, std::byte {1})).get().has_value());
            EXPECT_TRUE(IsError(_connection->Send({std::byte {99}}).get(), Error::SendQueueFull));
            _peer.set_option(Tcp::socket::linger(true, 0));
            _peer.close();
        }));
        const std::weak_ptr weakConnection = _connection;
        _connection.reset();
        ASSERT_TRUE(PumpUntil(_context, [&] -> bool { return weakConnection.expired(); }));
        const auto replacement = _manager.CreateConnection(Tcp::socket(_strand));
        EXPECT_TRUE(replacement.has_value());
    }
}   // namespace Flux::Test
