#include "Networking/ConnectionManager.hpp"

#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <barrier>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/system/system_error.hpp>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "Networking/Connection.hpp"
#include "Networking/IoContextPool.hpp"
#include "Support/Async.hpp"
#include "Support/FailingExecutor.hpp"

namespace Flux::Test
{
    namespace
    {
        class TestProtocolSession final : public IProtocolSession
        {
        public:
            auto OnBytes(std::span<const std::byte> /*bytes*/) -> bool override { return true; }
        };

        auto ExpectPeerClosed(boost::asio::io_context& context, boost::asio::ip::tcp::socket& peer)
            -> void
        {
            char byte = 0;
            ErrorCode readError;
            std::size_t bytesRead = 1;
            bool completed = false;
            peer.async_read_some(boost::asio::buffer(&byte, 1),
                                 [&](const ErrorCode& error, const std::size_t size) -> void {
                                     readError = error;
                                     bytesRead = size;
                                     completed = true;
                                 });
            ASSERT_TRUE(PumpUntil(context, [&]() -> bool { return completed; }));
            EXPECT_EQ(bytesRead, 0U);
            EXPECT_TRUE(readError == boost::asio::error::eof
                        || readError == boost::asio::error::connection_reset)
                << readError.message();
        }
    }   // namespace

    static_assert(!std::is_copy_constructible_v<ConnectionManager>);
    static_assert(!std::is_move_constructible_v<ConnectionManager>);
    static_assert(!std::is_copy_assignable_v<ConnectionManager>);
    static_assert(!std::is_move_assignable_v<ConnectionManager>);

    TEST(ConnectionManagerTest, CreatesProtocolSessionUsingInjectedFactory)
    {
        std::size_t factoryCalls = 0;
        const SessionFactory sessionFactory = [&] -> std::unique_ptr<IProtocolSession> {
            ++factoryCalls;
            return std::make_unique<TestProtocolSession>();
        };
        boost::asio::io_context context;
        const ConnectionManager manager(1, ConnectionOptions(), sessionFactory);

        const auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context));

        ASSERT_TRUE(connection.has_value());
        EXPECT_EQ(factoryCalls, 1U);
    }

    TEST(ConnectionManagerTest, ConnectionFactoryPreservesDefaultAndCustomIdleTimeouts)
    {
        using namespace std::chrono_literals;
        boost::asio::io_context context;
        const ConnectionManager manager(3);
        const auto defaultConnection = manager.CreateConnection(
            boost::asio::ip::tcp::socket(context));
        const auto customConnection = manager.CreateConnection(
            boost::asio::ip::tcp::socket(context),
            7s);
        const auto boundaryConnection = manager.CreateConnection(
            boost::asio::ip::tcp::socket(context),
            1s);
        ASSERT_TRUE(defaultConnection.has_value());
        ASSERT_TRUE(customConnection.has_value());
        ASSERT_TRUE(boundaryConnection.has_value());
        EXPECT_EQ(Connection::IdleTimeoutDuration, 120s);
        EXPECT_EQ((*defaultConnection)->GetIdleTimeout(), 120s);
        EXPECT_EQ((*customConnection)->GetIdleTimeout(), 7s);
        EXPECT_EQ((*boundaryConnection)->GetIdleTimeout(), 1s);
        ASSERT_TRUE(customConnection.value()->SetIdleTimeout(9s).has_value());
        EXPECT_EQ((*customConnection)->GetIdleTimeout(), 9s);
        EXPECT_EQ((*defaultConnection)->GetIdleTimeout(), 120s);
    }

    TEST(ConnectionManagerTest, AppliesConfiguredConnectionOptionsToNewConnections)
    {
        using namespace std::chrono_literals;
        boost::asio::io_context context;
        constexpr ConnectionOptions options {
            .IdleTimeout = 7s,
            .MaxRequestSize = 1234,
            .MaxSendQueue = 5678,
        };
        const ConnectionManager manager(1, options);

        const auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context));

        ASSERT_TRUE(connection.has_value());
        EXPECT_EQ((*connection)->GetIdleTimeout(), 7s);
        EXPECT_EQ((*connection)->GetMaxRequestSize(), 1234U);
        EXPECT_EQ((*connection)->GetMaxSendQueue(), 5678U);
    }

    TEST(ConnectionManagerTest, RejectsNonPositiveIdleTimeoutOptions)
    {
        using namespace std::chrono_literals;
        const boost::asio::io_context context;

        for (const auto timeout : {std::chrono::seconds::min(), 0s, -1s})
        {
            const ConnectionOptions options {.IdleTimeout = timeout};
            EXPECT_THROW(ConnectionManager(1, options), std::invalid_argument);
        }
    }

    TEST(ConnectionManagerTest, PreservesMaximumConnectionOptionValues)
    {
        constexpr auto maximum = std::numeric_limits<std::size_t>::max();
        constexpr ConnectionOptions options {
            .IdleTimeout = std::chrono::seconds::max(),
            .MaxRequestSize = maximum,
            .MaxSendQueue = maximum,
        };
        boost::asio::io_context context;
        const ConnectionManager manager(1, options);
        const auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context));

        ASSERT_TRUE(connection.has_value());
        EXPECT_EQ((*connection)->GetIdleTimeout(), std::chrono::seconds::max());
        EXPECT_EQ((*connection)->GetMaxRequestSize(), maximum);
        EXPECT_EQ((*connection)->GetMaxSendQueue(), maximum);
    }

    TEST(ConnectionManagerTest, PreservesConnectionOptionBoundaries)
    {
        constexpr auto maximum = std::numeric_limits<std::size_t>::max();
        for (const auto limit : {std::size_t {0}, std::size_t {1}, maximum})
        {
            SCOPED_TRACE(limit);
            const ConnectionOptions options {
                .IdleTimeout = std::chrono::seconds {1},
                .MaxRequestSize = limit,
                .MaxSendQueue = limit,
            };
            boost::asio::io_context context;
            const ConnectionManager manager(1, options);
            const auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context));

            ASSERT_TRUE(connection.has_value());
            EXPECT_EQ((*connection)->GetMaxRequestSize(), limit);
            EXPECT_EQ((*connection)->GetMaxSendQueue(), limit);
        }
    }

    TEST(ConnectionManagerTest, RejectsNonPositivePerConnectionIdleTimeout)
    {
        using namespace std::chrono_literals;
        boost::asio::io_context context;
        const ConnectionManager manager(1);

        for (const auto timeout : {0s, -1s})
        {
            const auto result = manager.CreateConnection(boost::asio::ip::tcp::socket(context),
                                                         timeout);
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), MakeErrorCode(Error::InvalidArgument));
        }

        EXPECT_TRUE(manager.CreateConnection(boost::asio::ip::tcp::socket(context)).has_value());
    }

    TEST(ConnectionManagerTest, StartFactoryRejectsNonPositiveTimeoutWithoutConsumingCapacity)
    {
        using namespace std::chrono_literals;
        using Tcp = boost::asio::ip::tcp;
        boost::asio::io_context context;

        for (const auto timeout : {std::chrono::seconds::min(), -1s, 0s})
        {
            SCOPED_TRACE(timeout.count());
            const ConnectionManager manager(1);
            const auto result = manager.CreateAndStartConnection(Tcp::socket(context), timeout);
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), MakeErrorCode(Error::InvalidArgument));
            const auto available = manager.CreateConnection(Tcp::socket(context));
            ASSERT_TRUE(available.has_value());
        }
    }

    TEST(ConnectionManagerTest, StartFactoryAcceptsMinimumAndMaximumPositiveTimeouts)
    {
        using Tcp = boost::asio::ip::tcp;
        const auto maximumTimeout = std::chrono::seconds::max();

        for (const auto timeout : {std::chrono::seconds {1}, maximumTimeout})
        {
            SCOPED_TRACE(timeout.count());
            boost::asio::io_context context;
            Tcp::acceptor listener(context, {boost::asio::ip::address_v4::loopback(), 0});
            Tcp::socket peer(context);
            peer.connect(listener.local_endpoint());
            const ConnectionManager manager(1);

            const auto connection = OnExecutor(context, context.get_executor(), [&] -> auto {
                return manager.CreateAndStartConnection(listener.accept(), timeout);
            });

            ASSERT_TRUE(connection.has_value());
            EXPECT_EQ((*connection)->GetIdleTimeout(), timeout);
            EXPECT_TRUE(AwaitFuture(context, (*connection)->AsyncClose()).has_value());
        }
    }

    TEST(ConnectionManagerTest, StartFactoryUsesDefaultIdleTimeout)
    {
        using namespace std::chrono_literals;
        using Tcp = boost::asio::ip::tcp;
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        Tcp::acceptor listener(context, {boost::asio::ip::address_v4::loopback(), 0});
        Tcp::socket peer(context);
        peer.connect(listener.local_endpoint());
        const auto connection = OnExecutor(context, context.get_executor(), [&] -> auto {
            return manager.CreateAndStartConnection(listener.accept());
        });
        ASSERT_TRUE(connection.has_value());
        EXPECT_EQ((*connection)->GetIdleTimeout(), 120s);
        EXPECT_TRUE(AwaitFuture(context, (*connection)->Send({})).has_value());
        EXPECT_TRUE(AwaitFuture(context, (*connection)->AsyncClose()).has_value());
    }

    TEST(ConnectionManagerTest, CustomIdleTimeoutExpiresOnlyItsOwnConnectionAndReleasesCapacity)
    {
        using namespace std::chrono_literals;
        using Tcp = boost::asio::ip::tcp;
        boost::asio::io_context context;
        const ConnectionManager manager(2);
        Tcp::acceptor listener(context, {boost::asio::ip::address_v4::loopback(), 0});
        Tcp::socket firstPeer(context);
        Tcp::socket secondPeer(context);
        firstPeer.connect(listener.local_endpoint());
        const auto first = OnExecutor(context, context.get_executor(), [&] -> auto {
            return manager.CreateAndStartConnection(listener.accept(), 1s);
        });
        ASSERT_TRUE(first.has_value());
        secondPeer.connect(listener.local_endpoint());
        const auto second = OnExecutor(context, context.get_executor(), [&] -> auto {
            return manager.CreateAndStartConnection(listener.accept(), 5s);
        });
        ASSERT_TRUE(second.has_value());
        EXPECT_EQ((*first)->GetIdleTimeout(), 1s);
        EXPECT_EQ((*second)->GetIdleTimeout(), 5s);
        const auto full = manager.CreateConnection(Tcp::socket(context));
        ASSERT_FALSE(full.has_value());
        EXPECT_EQ(full.error(), MakeErrorCode(Error::ConnectionLimitReached));
        boost::asio::steady_timer wait(context, 1200ms);
        AwaitFuture(context, wait.async_wait(boost::asio::use_future));
        const auto expired = AwaitFuture(context, first.value()->Send({}));
        ASSERT_FALSE(expired.has_value());
        EXPECT_EQ(expired.error(), MakeErrorCode(Error::ConnectionClosed));
        ASSERT_NO_FATAL_FAILURE(ExpectPeerClosed(context, firstPeer));
        EXPECT_TRUE(AwaitFuture(context, (*second)->Send({})).has_value());
        const auto replacement = manager.CreateConnection(Tcp::socket(context));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_TRUE(OnExecutor(context, context.get_executor(), [&] -> Result {
                        return manager.CloseAllConnections();
                    }).has_value());
        EXPECT_TRUE(AwaitFuture(context, (*first)->AsyncClose()).has_value());
        context.restart();
        context.poll();
        EXPECT_TRUE(context.stopped());
    }

    TEST(ConnectionManagerTest, IdleTimeoutReleasesActiveConnectionAfterManagerDestruction)
    {
        using namespace std::chrono_literals;
        using Tcp = boost::asio::ip::tcp;
        boost::asio::io_context context;
        Tcp::acceptor listener(context, {boost::asio::ip::address_v4::loopback(), 0});
        Tcp::socket peer(context);
        peer.connect(listener.local_endpoint());
        std::weak_ptr<Connection> weakConnection;
        {
            const ConnectionManager manager(1);
            const auto connection = OnExecutor(context, context.get_executor(), [&] -> auto {
                return manager.CreateAndStartConnection(listener.accept(), 1s);
            });
            ASSERT_TRUE(connection.has_value());
            weakConnection = *connection;
        }
        EXPECT_FALSE(weakConnection.expired());
        ASSERT_TRUE(PumpUntil(context, [&] -> bool { return weakConnection.expired(); }));
        ASSERT_NO_FATAL_FAILURE(ExpectPeerClosed(context, peer));
        context.restart();
        context.poll();
        EXPECT_TRUE(context.stopped());
    }

    TEST(ConnectionManagerTest, CalculatesSmallerResourceBudgetWithSafetyMargin)
    {
        constexpr std::uint64_t memoryPerConnection = 256ULL * 1024;
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({164, 200 * memoryPerConnection}),
                  80U);
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({264, 50 * memoryPerConnection}), 40U);

        constexpr ConnectionLimitOptions options {
            .MemoryPerConnectionBytes = 10,
            .ReservedFileDescriptors = 2,
            .SafetyFactor = 0.5,
        };
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({9, 79}, options), 3U);
    }

    TEST(ConnectionManagerTest, ExplicitConnectionLimitCapsCalculatedCapacity)
    {
        constexpr ConnectionLimitOptions options {
            .MemoryPerConnectionBytes = 10,
            .ReservedFileDescriptors = 2,
            .SafetyFactor = 0.5,
        };
        constexpr SystemResources resources {
            .FileDescriptorLimit = 100,
            .AvailableMemoryBytes = 1000,
        };

        EXPECT_EQ(ConnectionManager::CalculateMaxConnections(resources, options, 17U), 17U);
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections(resources, options, 1000U), 49U);
    }

    TEST(ConnectionManagerTest, ExplicitConnectionLimitHonorsZeroOneAndMaximum)
    {
        constexpr ConnectionLimitOptions options {
            .MemoryPerConnectionBytes = 10,
            .ReservedFileDescriptors = 2,
            .SafetyFactor = 0.5,
        };
        constexpr SystemResources resources {
            .FileDescriptorLimit = 100,
            .AvailableMemoryBytes = 1000,
        };

        EXPECT_EQ(ConnectionManager::CalculateMaxConnections(resources, options, 0U), 0U);
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections(resources, options, 1U), 1U);
        EXPECT_EQ(
            ConnectionManager::CalculateMaxConnections(resources,
                                                       options,
                                                       std::numeric_limits<std::size_t>::max()),
            49U);
    }

    TEST(ConnectionManagerTest, ExhaustedResourceBudgetsProduceZeroCapacity)
    {
        constexpr auto memory = std::numeric_limits<std::uint64_t>::max();
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({0, memory}), 0U);
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({63, memory}), 0U);
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({64, memory}), 0U);
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({1000, 0}), 0U);
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({1000, (256ULL * 1024) - 1}), 0U);
    }

    TEST(ConnectionManagerTest, RejectsInvalidLimitOptions)
    {
        ConnectionLimitOptions options;
        options.MemoryPerConnectionBytes = 0;
        const auto zeroMemory = ConnectionManager::CalculateMaxConnections(
            {.FileDescriptorLimit = 1000, .AvailableMemoryBytes = 1000},
            options);
        ASSERT_FALSE(zeroMemory.has_value());
        EXPECT_EQ(zeroMemory.error(), MakeErrorCode(Error::InvalidArgument));

        options.MemoryPerConnectionBytes = 1;
        for (const double factor : {
                 0.0,
                 -0.1,
                 1.1,
                 -std::numeric_limits<double>::infinity(),
                 std::numeric_limits<double>::infinity(),
                 std::numeric_limits<double>::quiet_NaN(),
             })
        {
            SCOPED_TRACE(factor);
            options.SafetyFactor = factor;
            const auto result = ConnectionManager::CalculateMaxConnections(
                {.FileDescriptorLimit = 1000, .AvailableMemoryBytes = 1000},
                options);
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), MakeErrorCode(Error::InvalidArgument));
        }
    }

    TEST(ConnectionManagerTest, CalculatesCapacityWithoutOverflowAtIntegerLimits)
    {
        constexpr auto resourceLimit = std::numeric_limits<std::uint64_t>::max();
        constexpr auto capacityLimit = std::numeric_limits<std::size_t>::max();
        ConnectionLimitOptions options {
            .MemoryPerConnectionBytes = 1,
            .ReservedFileDescriptors = 0,
            .SafetyFactor = 1.0,
        };
        EXPECT_EQ(
            ConnectionManager::CalculateMaxConnections({resourceLimit, resourceLimit}, options),
            capacityLimit);
        constexpr double halfBudget = 0.5;
        options.SafetyFactor = halfBudget;
        // This large budget is exactly representable even where long double is 64 bits.
        constexpr auto exactBudget = resourceLimit - 2047;
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({exactBudget, exactBudget}, options),
                  std::min<std::uint64_t>(capacityLimit, exactBudget / 2));

        options.MemoryPerConnectionBytes = resourceLimit;
        options.SafetyFactor = 1.0;
        EXPECT_EQ(
            ConnectionManager::CalculateMaxConnections({resourceLimit, resourceLimit}, options),
            1U);
    }

    TEST(ConnectionManagerTest, RoundsDownMemoryBudgetBeforeApplyingSafetyFactor)
    {
        constexpr ConnectionLimitOptions options {
            .MemoryPerConnectionBytes = 10,
            .ReservedFileDescriptors = 0,
            .SafetyFactor = 0.75,
        };
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({100, 29}, options), 1U);
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({100, 9}, options), 0U);
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({1, 1000}, options), 0U);
    }

    TEST(ConnectionManagerTest, AcceptsSafetyFactorBoundariesAndRejectsAdjacentInvalidValues)
    {
        ConnectionLimitOptions options {
            .MemoryPerConnectionBytes = 1,
            .ReservedFileDescriptors = 0,
            .SafetyFactor = 1.0,
        };
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({10, 10}, options), 10U);
        options.SafetyFactor = std::nextafter(1.0, 0.0);
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({10, 10}, options), 9U);
        options.SafetyFactor = std::numeric_limits<double>::min();
        EXPECT_EQ(ConnectionManager::CalculateMaxConnections({10, 10}, options), 0U);

        for (const auto factor : {std::nextafter(1.0, 2.0), std::nextafter(0.0, -1.0)})
        {
            options.SafetyFactor = factor;
            const auto result = ConnectionManager::CalculateMaxConnections(
                {.FileDescriptorLimit = 10, .AvailableMemoryBytes = 10},
                options);
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), MakeErrorCode(Error::InvalidArgument));
        }
    }

    TEST(ConnectionManagerTest, FullDescriptorReservationDoesNotUnderflow)
    {
        constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
        constexpr ConnectionLimitOptions options {
            .MemoryPerConnectionBytes = 1,
            .ReservedFileDescriptors = maximum,
            .SafetyFactor = 1.0,
        };
        for (const auto descriptors : {std::uint64_t {0}, maximum - 1, maximum})
        {
            SCOPED_TRACE(descriptors);
            EXPECT_EQ(ConnectionManager::CalculateMaxConnections({descriptors, maximum}, options),
                      0U);
        }
    }

    TEST(ConnectionManagerTest, ManualConstructorPreservesCapacityIncludingIntegerMaximum)
    {
        for (const auto capacity :
             {std::size_t {0}, std::size_t {1}, std::numeric_limits<std::size_t>::max()})
        {
            const ConnectionManager manager(capacity);
            EXPECT_EQ(manager.GetMaxConnections(), capacity);
        }
    }

    TEST(ConnectionManagerTest, OptionsConstructorAppliesDescriptorReservation)
    {
        constexpr ConnectionLimitOptions options {
            .ReservedFileDescriptors = std::numeric_limits<std::uint64_t>::max(),
        };
        const ConnectionManager manager(options);
        EXPECT_EQ(manager.GetMaxConnections(), 0U);
        boost::asio::io_context context;
        const auto result = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), MakeErrorCode(Error::ConnectionLimitReached));
    }

    TEST(ConnectionManagerTest, ResourceAwareConstructorsHonorExplicitZeroCapacity)
    {
        constexpr ConnectionLimitOptions limits {
            .MemoryPerConnectionBytes = 1,
            .ReservedFileDescriptors = 0,
            .SafetyFactor = 1.0,
        };
        constexpr ConnectionOptions connectionOptions {
            .IdleTimeout = std::chrono::seconds {1},
            .MaxRequestSize = 1,
            .MaxSendQueue = 1,
        };
        const ConnectionManager withBothOptions(limits, connectionOptions, 0U);
        const ConnectionManager withConnectionOptions(connectionOptions, 0U);

        EXPECT_EQ(withBothOptions.GetMaxConnections(), 0U);
        EXPECT_EQ(withConnectionOptions.GetMaxConnections(), 0U);
    }

    TEST(ConnectionManagerTest, InvalidOptionsConstructorPreservesExpectedError)
    {
        for (const auto options : {
                 ConnectionLimitOptions {.MemoryPerConnectionBytes = 0},
                 ConnectionLimitOptions {.SafetyFactor = 0.0},
                 ConnectionLimitOptions {.SafetyFactor = -0.1},
                 ConnectionLimitOptions {.SafetyFactor = 1.1},
                 ConnectionLimitOptions {.SafetyFactor = std::numeric_limits<double>::infinity()},
                 ConnectionLimitOptions {.SafetyFactor = std::numeric_limits<double>::quiet_NaN()},
             })
        {
            SCOPED_TRACE(options.SafetyFactor);
            try
            {
                const ConnectionManager manager(options);
                FAIL() << "Invalid connection limits were accepted";
            }
            catch (const std::bad_expected_access<std::error_code>& error)
            {
                EXPECT_EQ(error.error(), MakeErrorCode(Error::InvalidArgument));
            }
        }
    }

    TEST(ConnectionManagerTest, ManualCapacityRejectsAndClosesExcessSocket)
    {
        using Tcp = boost::asio::ip::tcp;
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto connection = manager.CreateConnection(Tcp::socket(context));
        ASSERT_TRUE(connection.has_value());
        Tcp::acceptor listener(context, {boost::asio::ip::address_v4::loopback(), 0});
        Tcp::socket peer(context);
        peer.connect(listener.local_endpoint());
        auto rejected = listener.accept();
        const auto result = manager.CreateConnection(std::move(rejected));
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), MakeErrorCode(Error::ConnectionLimitReached));
        // Asio permits querying is_open() on a moved-from socket.
        // NOLINTNEXTLINE(bugprone-use-after-move)
        EXPECT_FALSE(rejected.is_open());

        EXPECT_TRUE(AwaitFuture(context, connection.value()->AsyncClose()).has_value());
        const auto replacement = manager.CreateConnection(Tcp::socket(context));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_NE(*replacement, nullptr);
        ExpectPeerClosed(context, peer);
    }

    TEST(ConnectionManagerTest, ZeroCapacityReturnsExpectedErrorAndClosesSocket)
    {
        using Tcp = boost::asio::ip::tcp;
        boost::asio::io_context context;
        const ConnectionManager manager(0);
        EXPECT_EQ(manager.GetMaxConnections(), 0U);
        Tcp::socket socket(context, Tcp::v4());
        const auto result = manager.CreateAndStartConnection(std::move(socket));
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), MakeErrorCode(Error::ConnectionLimitReached));
        // Asio permits querying is_open() on a moved-from socket.
        // NOLINTNEXTLINE(bugprone-use-after-move)
        EXPECT_FALSE(socket.is_open());
    }

    TEST(ConnectionManagerTest, ZeroCapacityRejectsCreationEvenForUnopenedSocket)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(0);
        for (const bool openSocket : {false, true})
        {
            SCOPED_TRACE(openSocket);
            boost::asio::ip::tcp::socket socket(context);
            if (openSocket)
                socket.open(boost::asio::ip::tcp::v4());
            const auto result = manager.CreateConnection(std::move(socket));
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), MakeErrorCode(Error::ConnectionLimitReached));
            // NOLINTNEXTLINE(bugprone-use-after-move)
            EXPECT_FALSE(socket.is_open());
        }
        EXPECT_TRUE(manager.CloseAllConnections().has_value());
    }

    TEST(ConnectionManagerTest, StartAtCapacityClosesRejectedPeerAndKeepsExistingConnection)
    {
        using Tcp = boost::asio::ip::tcp;
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto existing = manager.CreateConnection(Tcp::socket(context));
        ASSERT_TRUE(existing.has_value());
        Tcp::acceptor listener(context, {boost::asio::ip::address_v4::loopback(), 0});
        Tcp::socket peer(context);
        peer.connect(listener.local_endpoint());
        auto socket = listener.accept();
        const auto rejected = OnExecutor(
            context,
            context.get_executor(),
            [&] -> std::expected<ConnectionManager::ConnectionPtr, ErrorCode> {
                return manager.CreateAndStartConnection(std::move(socket));
            });
        ASSERT_FALSE(rejected.has_value());
        EXPECT_EQ(rejected.error(), MakeErrorCode(Error::ConnectionLimitReached));
        ASSERT_NO_FATAL_FAILURE(ExpectPeerClosed(context, peer));
        const auto send = AwaitFuture(context, existing.value()->Send({}));
        ASSERT_FALSE(send.has_value());
        EXPECT_EQ(send.error(), MakeErrorCode(Error::ConnectionNotStarted));
        const auto stillFull = manager.CreateConnection(Tcp::socket(context));
        ASSERT_FALSE(stillFull.has_value());
        EXPECT_EQ(stillFull.error(), MakeErrorCode(Error::ConnectionLimitReached));
        EXPECT_TRUE(AwaitFuture(context, (*existing)->AsyncClose()).has_value());
    }

    TEST(ConnectionManagerTest, RepeatedCloseDoesNotReleaseAnotherConnectionsSlot)
    {
        using Tcp = boost::asio::ip::tcp;
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto first = manager.CreateConnection(Tcp::socket(context));
        ASSERT_TRUE(first.has_value());
        ASSERT_TRUE(OnExecutor(context, context.get_executor(), [&] -> Result {
                        return manager.CloseConnection(*first);
                    }).has_value());
        const auto replacement = manager.CreateConnection(Tcp::socket(context));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_NE(*replacement, *first);
        EXPECT_TRUE(manager.CloseConnection(*first).has_value());
        EXPECT_TRUE(AwaitFuture(context, (*first)->AsyncClose()).has_value());
        const auto rejected = manager.CreateConnection(Tcp::socket(context));
        ASSERT_FALSE(rejected.has_value());
        EXPECT_EQ(rejected.error(), MakeErrorCode(Error::ConnectionLimitReached));
        EXPECT_EQ(manager.GetMaxConnections(), 1U);
    }

    TEST(ConnectionManagerTest, BulkCloseRestoresEntireCapacityWithOldReferencesStillAlive)
    {
        using Tcp = boost::asio::ip::tcp;
        constexpr std::size_t capacity = 3;
        boost::asio::io_context context;
        const ConnectionManager manager(capacity);
        std::vector<ConnectionManager::ConnectionPtr> retained;
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            SCOPED_TRACE(cycle);
            for (std::size_t index = 0; index < capacity; ++index)
            {
                const auto result = manager.CreateConnection(Tcp::socket(context));
                ASSERT_TRUE(result.has_value());
                retained.push_back(*result);
            }
            const auto rejected = manager.CreateConnection(Tcp::socket(context));
            ASSERT_FALSE(rejected.has_value());
            EXPECT_EQ(rejected.error(), MakeErrorCode(Error::ConnectionLimitReached));
            ASSERT_TRUE(OnExecutor(context, context.get_executor(), [&] -> Result {
                            return manager.CloseAllConnections();
                        }).has_value());
            EXPECT_TRUE(manager.CloseAllConnections().has_value());
            EXPECT_EQ(manager.GetMaxConnections(), capacity);
        }
        for (const auto& connection : retained)
        {
            const auto result = AwaitFuture(context, connection->AsyncStart());
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), MakeErrorCode(Error::ConnectionClosed));
        }
    }

    TEST(ConnectionManagerTest, ConcurrentCreationNeverExceedsConfiguredCapacity)
    {
        constexpr std::size_t capacity = 7;
        constexpr std::size_t workers = 8;
        constexpr std::size_t attemptsPerWorker = 4;
        boost::asio::io_context context;
        const ConnectionManager manager(capacity);
        std::array<ConnectionManager::ConnectionPtr, workers * attemptsPerWorker> connections;
        std::array<ErrorCode, workers * attemptsPerWorker> errors;
        std::barrier start(workers);
        std::vector<std::jthread> callers;
        callers.reserve(workers);
        for (std::size_t worker = 0; worker < workers; ++worker)
        {
            callers.emplace_back([&, worker] -> void {
                start.arrive_and_wait();
                for (std::size_t index = 0; index < attemptsPerWorker; ++index)
                {
                    const auto slot = (worker * attemptsPerWorker) + index;
                    if (auto result = manager.CreateConnection(
                            boost::asio::ip::tcp::socket(context)))
                    {
                        connections.at(slot) = std::move(*result);
                    }
                    else
                    {
                        errors.at(slot) = result.error();
                    }
                }
            });
        }
        callers.clear();
        const auto accepted = std::ranges::count_if(
            connections,
            [](const auto& connection) -> bool { return static_cast<bool>(connection); });
        const auto rejected = std::ranges::count(errors,
                                                 MakeErrorCode(Error::ConnectionLimitReached));
        EXPECT_EQ(accepted, capacity);
        EXPECT_EQ(rejected, connections.size() - capacity);
    }

    TEST(ConnectionManagerTest, EmptyAndForeignConnectionsAreNoOps)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const ConnectionManager other(1);
        const auto owned = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(owned.has_value());
        const auto foreign = other.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(foreign.has_value());
        EXPECT_TRUE(manager.CloseConnection(nullptr).has_value());
        EXPECT_TRUE(manager.CloseConnection(foreign.value()).has_value());
        const auto stillFull = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_FALSE(stillFull.has_value());
        EXPECT_EQ(stillFull.error(), MakeErrorCode(Error::ConnectionLimitReached));
        EXPECT_TRUE(OnExecutor(context, context.get_executor(), [&] -> Result {
                        return manager.CloseAllConnections();
                    }).has_value());
        const auto result = AwaitFuture(context, foreign.value()->Send({}));
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), MakeErrorCode(Error::ConnectionNotStarted));
        EXPECT_TRUE(AwaitFuture(context, foreign.value()->AsyncClose()).has_value());
    }

    TEST(ConnectionManagerTest, RetainsCreatedConnectionUntilExplicitClose)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(connection.has_value());
        const std::weak_ptr weakConnection = connection.value();
        connection.value().reset();
        ASSERT_FALSE(weakConnection.expired());
        const auto rejected = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_FALSE(rejected.has_value());
        EXPECT_EQ(rejected.error(), MakeErrorCode(Error::ConnectionLimitReached));
        const auto closed = OnExecutor(context, context.get_executor(), [&] -> Result {
            return manager.CloseConnection(weakConnection.lock());
        });
        EXPECT_TRUE(closed.has_value());
        EXPECT_TRUE(weakConnection.expired());
    }

    TEST(ConnectionManagerTest, DirectCloseRemovesConnectionFromManager)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(connection.has_value());
        const std::weak_ptr weakConnection = connection.value();
        ASSERT_TRUE(AwaitFuture(context, connection.value()->AsyncClose()).has_value());
        connection.value().reset();
        EXPECT_TRUE(weakConnection.expired());
        EXPECT_TRUE(manager.CloseAllConnections().has_value());
    }

    TEST(ConnectionManagerTest, DestructionReleasesUnstartedConnections)
    {
        std::weak_ptr<Connection> connection;
        {
            boost::asio::io_context context;
            const ConnectionManager manager(1);
            const auto result = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
            ASSERT_TRUE(result.has_value());
            connection = *result;
            EXPECT_FALSE(connection.expired());
        }
        EXPECT_TRUE(connection.expired());
    }

    TEST(ConnectionManagerTest, CloseFromControlThreadRunsCallbackOnWorker)
    {
        IoContextPool pool(1);
        ASSERT_TRUE(pool.Run().has_value());
        const ConnectionManager manager(1);
        auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(pool.GetStrand()));
        ASSERT_TRUE(connection.has_value());
        const std::weak_ptr weakConnection = connection.value();
        EXPECT_TRUE(manager.CloseConnection(connection.value()).has_value());
        connection.value().reset();
        EXPECT_TRUE(pool.Stop().has_value());
        EXPECT_TRUE(weakConnection.expired());
    }

    TEST(ConnectionManagerTest, ConcurrentCreationAndCloseReleaseEveryConnection)
    {
        IoContextPool pool(2);
        ASSERT_TRUE(pool.Run().has_value());
        constexpr std::size_t workerCount = 4;
        constexpr std::size_t connectionsPerWorker = 20;
        constexpr auto connectionCount = workerCount * connectionsPerWorker;
        const ConnectionManager manager(connectionCount);
        std::vector<std::jthread> callers;
        callers.reserve(workerCount);
        std::vector<std::weak_ptr<Connection>> connections(connectionCount);
        for (std::size_t worker = 0; worker < workerCount; ++worker)
        {
            callers.emplace_back([&, worker] -> void {
                for (std::size_t index = 0; index < connectionsPerWorker; ++index)
                {
                    auto connection = manager.CreateConnection(
                        boost::asio::ip::tcp::socket(pool.GetStrand()));
                    ASSERT_TRUE(connection.has_value());
                    connections.at((worker * connectionsPerWorker) + index) = connection.value();
                    EXPECT_TRUE(manager.CloseConnection(connection.value()).has_value());
                    connection.value().reset();
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
        constexpr std::size_t connectionCount = 5;
        const ConnectionManager manager(connectionCount);
        std::vector<Tcp::socket> peers;
        std::vector<std::weak_ptr<Connection>> connections;
        peers.reserve(connectionCount);
        connections.reserve(connectionCount);
        for (std::size_t index = 0; index < connectionCount; ++index)
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
                                 [&](const ErrorCode& readError, std::size_t size) -> void {
                                     error = readError;
                                     EXPECT_EQ(size, 0U);
                                     completed = true;
                                 });
            ASSERT_TRUE(PumpUntil(clients, [&] -> bool { return completed; }));
            EXPECT_TRUE(error == boost::asio::error::eof
                        || error == boost::asio::error::connection_reset)
                << error.message();
        }
    }

    TEST(ConnectionManagerTest, StartReturnsErrorForUnopenedSocket)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto result = OnExecutor(context, context.get_executor(), [&] -> auto {
            return manager.CreateAndStartConnection(boost::asio::ip::tcp::socket(context));
        });
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), ErrorCode(boost::asio::error::bad_descriptor));
        const auto replacement = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_NE(*replacement, nullptr);
        EXPECT_TRUE(OnExecutor(context, context.get_executor(), [&]() -> Result {
                        return manager.CloseAllConnections();
                    }).has_value());
    }

    TEST(ConnectionManagerTest, StartSchedulingExceptionReleasesCapacityAndClosesSocket)
    {
        using Tcp = boost::asio::ip::tcp;
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto failure = std::make_shared<std::exception_ptr>(
            std::make_exception_ptr(std::bad_alloc {}));
        Tcp::acceptor listener(context, {boost::asio::ip::address_v4::loopback(), 0});
        Tcp::socket peer(context);
        peer.connect(listener.local_endpoint());
        Tcp::socket socket(FailingExecutor(context.get_executor(), failure));
        listener.accept(socket);

        EXPECT_THROW(
            {
                [[maybe_unused]] const auto result = manager.CreateAndStartConnection(
                    std::move(socket));
            },
            std::bad_alloc);
        EXPECT_FALSE(*failure);
        ASSERT_NO_FATAL_FAILURE(ExpectPeerClosed(context, peer));
        const auto replacement = manager.CreateConnection(Tcp::socket(context));
        ASSERT_TRUE(replacement.has_value());
    }

    TEST(ConnectionManagerTest, CloseSchedulingExceptionKeepsConnectionManagedForRetry)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto failure = std::make_shared<std::exception_ptr>(
            std::make_exception_ptr(std::bad_alloc {}));
        auto connection = manager.CreateConnection(
            boost::asio::ip::tcp::socket(FailingExecutor(context.get_executor(), failure)));
        ASSERT_TRUE(connection.has_value());
        const std::weak_ptr weakConnection = *connection;
        EXPECT_THROW(
            { [[maybe_unused]] const auto result = manager.CloseConnection(*connection); },
            std::bad_alloc);
        EXPECT_FALSE(*failure);
        connection->reset();
        EXPECT_FALSE(weakConnection.expired());
        const auto rejected = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_FALSE(rejected.has_value());
        EXPECT_EQ(rejected.error(), MakeErrorCode(Error::ConnectionLimitReached));
        EXPECT_TRUE(AwaitFuture(context, weakConnection.lock()->AsyncClose()).has_value());
        EXPECT_TRUE(weakConnection.expired());
    }

    TEST(ConnectionManagerTest, BulkClosePreservesSystemErrorAndClosesRemainingConnections)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(2);
        const ErrorCode expected = boost::asio::error::no_memory;
        const auto failure = std::make_shared<std::exception_ptr>(
            std::make_exception_ptr(boost::system::system_error(expected)));
        const auto failing = manager.CreateConnection(
            boost::asio::ip::tcp::socket(FailingExecutor(context.get_executor(), failure)));
        auto healthy = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(failing.has_value());
        ASSERT_TRUE(healthy.has_value());
        const std::weak_ptr weakHealthy = *healthy;
        healthy->reset();

        const auto closed = OnExecutor(context, context.get_executor(), [&] -> Result {
            return manager.CloseAllConnections();
        });
        ASSERT_FALSE(closed.has_value());
        EXPECT_EQ(closed.error(), expected);
        EXPECT_FALSE(*failure);
        EXPECT_TRUE(weakHealthy.expired());
        const auto replacement = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(replacement.has_value());
        EXPECT_TRUE(AwaitFuture(context, (*failing)->AsyncClose()).has_value());
    }

    TEST(ConnectionManagerTest, BulkCloseConvertsStandardAndUnknownExceptionsAndCanBeRetried)
    {
        struct NonStandardFailure
        {};

        for (const auto& exception : {
                 std::make_exception_ptr(std::bad_alloc {}),
                 std::make_exception_ptr(NonStandardFailure {}),
             })
        {
            boost::asio::io_context context;
            const ConnectionManager manager(2);
            std::array<std::shared_ptr<std::exception_ptr>, 2> failures;
            std::array<std::weak_ptr<Connection>, 2> connections;
            for (std::size_t index = 0; index < failures.size(); ++index)
            {
                failures.at(index) = std::make_shared<std::exception_ptr>(exception);
                const auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(
                    FailingExecutor(context.get_executor(), failures.at(index))));
                ASSERT_TRUE(connection.has_value());
                connections.at(index) = *connection;
            }
            const auto closed = manager.CloseAllConnections();
            ASSERT_FALSE(closed.has_value());
            EXPECT_EQ(closed.error(), MakeErrorCode(Error::ConnectionException));
            for (const auto& failure : failures) EXPECT_FALSE(*failure);
            for (const auto& connection : connections) EXPECT_FALSE(connection.expired());
            const auto retried = OnExecutor(context, context.get_executor(), [&] -> Result {
                return manager.CloseAllConnections();
            });
            EXPECT_TRUE(retried.has_value());
            for (const auto& connection : connections) EXPECT_TRUE(connection.expired());
            const auto replacement = manager.CreateConnection(
                boost::asio::ip::tcp::socket(context));
            ASSERT_TRUE(replacement.has_value());
        }
    }

    TEST(ConnectionManagerTest, StartsAndClosesConnectedSocket)
    {
        using Tcp = boost::asio::ip::tcp;
        boost::asio::io_context context;
        Tcp::acceptor acceptor(context, {boost::asio::ip::address_v4::loopback(), 0});
        Tcp::socket client(context);
        client.connect(acceptor.local_endpoint());
        auto socket = acceptor.accept();
        const ConnectionManager manager(1);

        boost::asio::post(context, [&] -> void {
            auto result = manager.CreateAndStartConnection(std::move(socket));
            ASSERT_TRUE(result.has_value());
            const auto& connection = *result;

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
        const ConnectionManager manager(2);
        auto first = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        auto second = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(first.has_value());
        ASSERT_TRUE(second.has_value());
        const std::weak_ptr weakFirst = first.value();
        const std::weak_ptr weakSecond = second.value();

        boost::asio::post(context, [&] -> auto {
            EXPECT_TRUE(manager.CloseAllConnections().has_value());
            const auto firstStart = first.value()->AsyncStart().get();
            const auto secondStart = second.value()->AsyncStart().get();
            ASSERT_FALSE(firstStart.has_value());
            ASSERT_FALSE(secondStart.has_value());
            EXPECT_EQ(firstStart.error(), MakeErrorCode(Error::ConnectionClosed));
            EXPECT_EQ(secondStart.error(), MakeErrorCode(Error::ConnectionClosed));
        });
        context.run();

        first.value().reset();
        second.value().reset();
        EXPECT_TRUE(weakFirst.expired());
        EXPECT_TRUE(weakSecond.expired());
        EXPECT_TRUE(manager.CloseAllConnections().has_value());
    }

    TEST(ConnectionManagerNoProtocolTest, CreatesConnectionWithoutSessionFactory)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(connection.has_value());
        EXPECT_NE(connection.value(), nullptr);
    }

    TEST(ConnectionManagerNoProtocolTest, CreatesMultipleConnectionsWithoutSessionFactory)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(3);
        auto first = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        auto second = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        auto third = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(first.has_value());
        ASSERT_TRUE(second.has_value());
        ASSERT_TRUE(third.has_value());
        EXPECT_NE(first.value(), second.value());
        EXPECT_NE(second.value(), third.value());
    }

    TEST(ConnectionManagerNoProtocolTest, CreateConnectionWithCustomIdleTimeoutWithoutFactory)
    {
        using namespace std::chrono_literals;
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context),
                                                         45s);
        ASSERT_TRUE(connection.has_value());
        EXPECT_EQ(connection.value()->GetIdleTimeout(), 45s);
    }

    TEST(ConnectionManagerNoProtocolTest, CreateConnectionRejectsZeroIdleTimeoutWithoutFactory)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context),
                                                         std::chrono::seconds::zero());
        ASSERT_FALSE(connection.has_value());
        EXPECT_EQ(connection.error(), MakeErrorCode(Error::InvalidArgument));
    }

    TEST(ConnectionManagerNoProtocolTest, CreateConnectionRejectsNegativeIdleTimeoutWithoutFactory)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context),
                                                         std::chrono::seconds {-1});
        ASSERT_FALSE(connection.has_value());
        EXPECT_EQ(connection.error(), MakeErrorCode(Error::InvalidArgument));
    }

    TEST(ConnectionManagerNoProtocolTest, ConnectionLimitReachedWithoutFactory)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto first = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(first.has_value());

        boost::asio::ip::tcp::socket socket(context);
        const auto second = manager.CreateConnection(std::move(socket));
        ASSERT_FALSE(second.has_value());
        EXPECT_EQ(second.error(), MakeErrorCode(Error::ConnectionLimitReached));
    }

    TEST(ConnectionManagerNoProtocolTest, FactoryReturningNullCreatesConnectionWithNullProtocol)
    {
        const SessionFactory nullFactory = [] -> std::unique_ptr<IProtocolSession> {
            return nullptr;
        };
        boost::asio::io_context context;
        const ConnectionManager manager(1, ConnectionOptions(), nullFactory);
        const auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(connection.has_value());
        EXPECT_NE(connection.value(), nullptr);
    }

    TEST(ConnectionManagerNoProtocolTest, CloseConnectionWithoutFactory)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(1);
        const auto connection = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(connection.has_value());

        boost::asio::post(context, [&] -> void {
            EXPECT_TRUE(manager.CloseConnection(connection.value()).has_value());
        });
        context.run();
    }

    TEST(ConnectionManagerNoProtocolTest, CloseAllConnectionsWithoutFactory)
    {
        boost::asio::io_context context;
        const ConnectionManager manager(2);
        const auto first = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        const auto second = manager.CreateConnection(boost::asio::ip::tcp::socket(context));
        ASSERT_TRUE(first.has_value());
        ASSERT_TRUE(second.has_value());

        boost::asio::post(context,
                          [&] -> void { EXPECT_TRUE(manager.CloseAllConnections().has_value()); });
        context.run();
    }

    TEST(ConnectionManagerNoProtocolTest, GetMaxConnectionsWithoutFactory)
    {
        const ConnectionManager manager(5);
        EXPECT_EQ(manager.GetMaxConnections(), 5U);
    }

}   // namespace Flux::Test
