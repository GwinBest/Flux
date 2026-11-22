#include "Server/Server.hpp"

#include "gtest/gtest.h"

#include <algorithm>
#include <barrier>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <expected>
#include <future>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

#include "Protocol/Http/HttpProtocolSession.hpp"
#include "Support/Async.hpp"
#include "Support/Socket.hpp"

namespace Flux::Test
{
    namespace
    {
        class TestProtocolSession final : public IProtocolSession
        {
        public:
            auto OnBytes(const std::span<const std::byte> /*bytes*/) -> bool override
            {
                return true;
            }
        };

        auto LoopbackEndpoint() -> Endpoint
        {
            return {boost::asio::ip::address_v4::loopback(), 0};
        }

        auto MakeServerOptions(const std::size_t threadCount, const Endpoint& endpoint)
            -> ServerOptions
        {
            ServerOptions options;
            options.ThreadCount = threadCount;
            options.BindEndpoint = endpoint;
            return options;
        }

        [[noreturn]] auto FailChild(const char* message) -> void
        {
            std::fputs(message, stderr);
            std::fflush(stderr);
            std::_Exit(EXIT_FAILURE);
        }

        [[noreturn]] auto FinishChildAfterShutdown() -> void
        {
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            std::exit(EXIT_SUCCESS);
        }

        // Bound the whole child, including synchronous calls and destructors. A CTest
        // timeout can otherwise leave the death-test child holding the executable open.
        template<typename Operation>
        [[noreturn]] auto RunChildWithDeadline(Operation operation) -> void
        {
            std::promise<void> completed;
            std::jthread watchdog([completion = completed.get_future()] -> auto {
                constexpr auto childTimeout = std::chrono::seconds(10);
                if (completion.wait_for(childTimeout) != std::future_status::ready)
                    FailChild("Server lifecycle exceeded the child deadline\n");
            });
            try
            {
                operation();
            }
            catch (const std::exception& error)
            {
                FailChild(error.what());
            }
            catch (...)
            {
                FailChild("Server lifecycle threw an unknown exception\n");
            }
            completed.set_value();
            watchdog.join();
            FinishChildAfterShutdown();
        }

        auto SignalLifecycle(const int signalNumber,
                             const bool explicitStop,
                             const std::size_t threadCount = 2) -> void
        {
            Server server(MakeServerOptions(threadCount, LoopbackEndpoint()));
            if (std::raise(signalNumber) != 0)
                FailChild("Failed to raise shutdown signal\n");
            if (!server.Run())
                FailChild("Server run failed\n");
            if (explicitStop)
            {
                if (!server.Stop())
                    FailChild("Server stop failed\n");
                if (const auto stopAgain = server.Stop();
                    stopAgain || stopAgain.error() != MakeErrorCode(Error::ServerNotStopping))
                    FailChild("Repeated stop returned an unexpected result\n");
                if (const auto runAgain = server.Run();
                    runAgain || runAgain.error() != MakeErrorCode(Error::ServerNotInitialized))
                    FailChild("Restart returned an unexpected result\n");
            }
        }

        auto WaitForRejectedRuns(const std::vector<std::future<Result>>& results) -> void
        {
            using namespace std::chrono_literals;
            const auto deadline = std::chrono::steady_clock::now() + 3s;
            for (;;)
            {
                std::size_t ready = 0;
                for (const auto& result : results)
                    ready += result.wait_for(0s) == std::future_status::ready ? 1U : 0U;
                if (ready == results.size())
                    FailChild("Concurrent Run stopped the winning invocation\n");
                if (ready == results.size() - 1)
                    return;
                if (std::chrono::steady_clock::now() >= deadline)
                    FailChild("Concurrent Run calls did not reject promptly\n");
                std::this_thread::yield();
            }
        }

        auto ConcurrentLifecycle(const bool raceRun) -> void
        {
            using namespace std::chrono_literals;
            constexpr std::size_t callers = 8;
            Server server(MakeServerOptions(2, LoopbackEndpoint()));
            if (!raceRun)
            {
                if (std::raise(SIGTERM) != 0 || !server.Run())
                    FailChild("Failed to prepare concurrent stop test\n");
            }

            std::barrier start(static_cast<std::ptrdiff_t>(callers + 1));
            std::vector<std::future<Result>> results;
            results.reserve(callers);
            for (std::size_t index = 0; index < callers; ++index)
            {
                results.push_back(std::async(std::launch::async, [&] -> Result {
                    start.arrive_and_wait();
                    return raceRun ? server.Run() : server.Stop();
                }));
            }
            start.arrive_and_wait();

            if (raceRun)
            {
                WaitForRejectedRuns(results);
                if (std::raise(SIGTERM) != 0)
                    FailChild("Failed to signal concurrent Run winner\n");
            }

            std::size_t successful = 0;
            for (auto& future : results)
            {
                if (future.wait_for(3s) != std::future_status::ready)
                    FailChild("Concurrent lifecycle call did not finish\n");
                if (const auto result = future.get())
                    ++successful;
                else if (result.error()
                         != MakeErrorCode(raceRun ? Error::ServerNotInitialized
                                                  : Error::ServerNotStopping))
                    FailChild("Concurrent lifecycle call reached a lower-level operation\n");
            }
            if (successful != 1)
                FailChild("Concurrent lifecycle calls must have exactly one winner\n");
            if (raceRun && !server.Stop())
                FailChild("Failed to stop concurrent Run winner\n");
        }

        auto StopAfterStartup(Server& server, const bool signalShutdown) -> Result
        {
            using namespace std::chrono_literals;
            // Serving a client can precede Run() publishing Status::Running.
            // Only retry the startup rejection; every other error is a failure.
            const auto deadline = std::chrono::steady_clock::now() + 3s;
            for (;;)
            {
                auto result = server.Stop();
                if (result || signalShutdown
                    || result.error() != MakeErrorCode(Error::ServerNotStopping)
                    || std::chrono::steady_clock::now() >= deadline)
                    return result;
                std::this_thread::yield();
            }
        }

        auto RunningServerLifecycle(const bool keepClientOpen, const bool signalShutdown = true)
            -> void
        {
            using namespace std::chrono_literals;
            using Tcp = boost::asio::ip::tcp;

            boost::asio::io_context clientContext;
            Tcp::acceptor reservation(clientContext, LoopbackEndpoint());
            const auto endpoint = reservation.local_endpoint();
            reservation.close();
            Server server(MakeServerOptions(1, endpoint));
            auto run = std::async(std::launch::async, [&server] -> Result { return server.Run(); });

            Tcp::socket client(clientContext);
            client.connect(endpoint);
            constexpr std::string_view payload = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
            auto written = boost::asio::async_write(client,
                                                    boost::asio::buffer(payload),
                                                    boost::asio::use_future);
            if (AwaitFuture(clientContext, std::move(written)) != payload.size())
                FailChild("Client payload was not written completely\n");
            char byte = '\0';
            ErrorCode readError;
            bool readCompleted = false;
            client.async_read_some(boost::asio::buffer(&byte, 1),
                                   [&](const ErrorCode& error, std::size_t size) -> void {
                                       readError = error;
                                       if (size != 0)
                                           FailChild("Unexpected server payload\n");
                                       readCompleted = true;
                                   });

            if (!keepClientOpen)
            {
                client.shutdown(Tcp::socket::shutdown_send);
                if (!PumpUntil(clientContext, [&] -> bool { return readCompleted; })
                    || readError != boost::asio::error::eof)
                    FailChild("Server did not close after client EOF\n");
            }
            if (run.wait_for(0s) != std::future_status::timeout)
                FailChild("Server run returned before shutdown was requested\n");

            if (signalShutdown)
            {
                if (std::raise(SIGTERM) != 0)
                    FailChild("Failed to raise shutdown signal\n");
                if (run.wait_for(3s) != std::future_status::ready)
                    FailChild("Server run did not return after the signal\n");
            }
            auto stopped = std::async(std::launch::async, [&] -> Result {
                return StopAfterStartup(server, signalShutdown);
            });
            if (stopped.wait_for(3s) != std::future_status::ready || !stopped.get())
                FailChild("Server stop did not complete successfully\n");
            if (run.wait_for(3s) != std::future_status::ready || !run.get())
                FailChild("Server run did not complete successfully after shutdown\n");

            if (const auto stopAgain = server.Stop();
                stopAgain || stopAgain.error() != MakeErrorCode(Error::ServerNotStopping))
                FailChild("Repeated stop returned an unexpected result\n");
            if (const auto runAgain = server.Run();
                runAgain || runAgain.error() != MakeErrorCode(Error::ServerNotInitialized))
                FailChild("Restart returned an unexpected result\n");
            if (keepClientOpen
                && (!PumpUntil(clientContext, [&] -> bool { return readCompleted; })
                    || (readError != boost::asio::error::eof
                        && readError != boost::asio::error::connection_reset)))
                FailChild("Server shutdown did not close active client\n");
        }
    }   // namespace

    static_assert(!std::is_copy_constructible_v<Server>);
    static_assert(!std::is_copy_assignable_v<Server>);
    static_assert(!std::is_move_constructible_v<Server>);
    static_assert(!std::is_move_assignable_v<Server>);
    static_assert(std::is_destructible_v<Server>);

    TEST(ServerOptionsTest, DefaultsToNoProtocol)
    {
        const ServerOptions options;
        const auto session = options.GetProtocol();
        EXPECT_EQ(session, nullptr);
    }

    TEST(ServerOptionsTest, GetProtocolFactoryReturnsEmptyByDefault)
    {
        const ServerOptions options;
        const auto& factory = options.GetProtocolFactory();
        EXPECT_FALSE(factory);
    }

    TEST(ServerOptionsTest, SetProtocolCreatesConfiguredSession)
    {
        ServerOptions options;
        options.SetProtocol<TestProtocolSession>();

        const auto session = options.GetProtocol();

        ASSERT_NE(session, nullptr);
        EXPECT_NE(dynamic_cast<TestProtocolSession*>(session.get()), nullptr);
    }

    TEST(ServerOptionsTest, SetProtocolUpdatesFactory)
    {
        ServerOptions options;
        EXPECT_EQ(options.GetProtocol(), nullptr);

        options.SetProtocol<TestProtocolSession>();
        ASSERT_NE(options.GetProtocol(), nullptr);
        EXPECT_NE(dynamic_cast<TestProtocolSession*>(options.GetProtocol().get()), nullptr);
    }

    TEST(ServerOptionsTest, SetProtocolWithHttpProtocolSession)
    {
        ServerOptions options;
        options.SetProtocol<HttpProtocolSession>(std::make_shared<HttpRouter>());

        const auto session = options.GetProtocol();
        ASSERT_NE(session, nullptr);
        EXPECT_NE(dynamic_cast<HttpProtocolSession*>(session.get()), nullptr);
    }

    TEST(ServerConstructorTest, NormalizesZeroThreadCount)
    {
        const Server server(MakeServerOptions(0, LoopbackEndpoint()));
        EXPECT_EQ(server.GetThreadCount(), 1U);
    }

    TEST(ServerConstructorTest, PreservesPositiveThreadCount)
    {
        const Server server(MakeServerOptions(3, LoopbackEndpoint()));
        EXPECT_EQ(server.GetThreadCount(), 3U);
    }

    TEST(ServerConstructorTest, RejectsNonPositiveIdleTimeout)
    {
        for (const auto timeout :
             {std::chrono::seconds::min(), std::chrono::seconds {-1}, std::chrono::seconds::zero()})
        {
            SCOPED_TRACE(timeout.count());
            ServerOptions options;
            options.BindEndpoint = LoopbackEndpoint();
            options.ConnectionConfig.IdleTimeout = timeout;

            EXPECT_THROW(Server {options}, std::invalid_argument);
        }
    }

    TEST(ServerConstructorTest, ExposesOptionsUsedToConfigureServer)
    {
        constexpr std::size_t threadCount = 3;
        constexpr std::size_t maxConnections = 42;
        constexpr auto idleTimeout = std::chrono::seconds(9);
        constexpr std::size_t maxRequestSize = 2048;
        constexpr std::size_t maxSendQueue = 4096;

        ServerOptions options;
        options.ThreadCount = threadCount;
        options.BindEndpoint = LoopbackEndpoint();
        options.MaxConnections = maxConnections;
        options.ConnectionConfig.IdleTimeout = idleTimeout;
        options.ConnectionConfig.MaxRequestSize = maxRequestSize;
        options.ConnectionConfig.MaxSendQueue = maxSendQueue;

        const Server server(options);

        EXPECT_EQ(server.GetThreadCount(), threadCount);
        EXPECT_EQ(server.GetOptions().BindEndpoint, options.BindEndpoint);
        EXPECT_EQ(server.GetOptions().MaxConnections, maxConnections);
        EXPECT_LE(server.GetMaxConnections(), maxConnections);
        EXPECT_EQ(server.GetOptions().ConnectionConfig.IdleTimeout, idleTimeout);
        EXPECT_EQ(server.GetOptions().ConnectionConfig.MaxRequestSize, maxRequestSize);
        EXPECT_EQ(server.GetOptions().ConnectionConfig.MaxSendQueue, maxSendQueue);
    }

    TEST(ServerConstructorTest, DefaultsToCalculatedConnectionCapacity)
    {
        ServerOptions options;
        options.ThreadCount = 1;
        options.BindEndpoint = LoopbackEndpoint();
        const Server server(options);

        EXPECT_GT(server.GetMaxConnections(), 0U);
    }

    TEST(ServerConstructorTest, PreservesThreadAndConnectionLimitBoundaries)
    {
        constexpr auto maximum = std::numeric_limits<std::size_t>::max();
        for (const auto threadCount : {std::size_t {0}, std::size_t {1}})
        {
            for (const auto connectionLimit : {std::size_t {0}, std::size_t {1}, maximum})
            {
                SCOPED_TRACE(testing::Message()
                             << "threads=" << threadCount << ", connections=" << connectionLimit);
                ServerOptions options;
                options.ThreadCount = threadCount;
                options.BindEndpoint = LoopbackEndpoint();
                options.MaxConnections = connectionLimit;
                const Server server(options);
                EXPECT_EQ(server.GetThreadCount(), std::max<std::size_t>(1, threadCount));
                EXPECT_EQ(server.GetOptions().MaxConnections, connectionLimit);
                EXPECT_LE(server.GetMaxConnections(), connectionLimit);
                if (connectionLimit == 0)
                {
                    EXPECT_EQ(server.GetMaxConnections(), 0U);
                }
            }
        }
    }

    TEST(ServerConstructorTest, PreservesConnectionOptionBoundaries)
    {
        constexpr auto maximum = std::numeric_limits<std::size_t>::max();
        for (const auto timeout : {std::chrono::seconds {1}, std::chrono::seconds::max()})
        {
            for (const auto requestLimit : {std::size_t {0}, std::size_t {1}, maximum})
            {
                for (const auto sendLimit : {std::size_t {0}, std::size_t {1}, maximum})
                {
                    SCOPED_TRACE(testing::Message() << "timeout=" << timeout.count() << ", request="
                                                    << requestLimit << ", send=" << sendLimit);
                    auto options = MakeServerOptions(1, LoopbackEndpoint());
                    options.MaxConnections = 1;
                    options.ConnectionConfig.IdleTimeout = timeout;
                    options.ConnectionConfig.MaxRequestSize = requestLimit;
                    options.ConnectionConfig.MaxSendQueue = sendLimit;
                    const Server server(options);
                    const auto& actual = server.GetOptions().ConnectionConfig;
                    EXPECT_EQ(actual.IdleTimeout, timeout);
                    EXPECT_EQ(actual.MaxRequestSize, requestLimit);
                    EXPECT_EQ(actual.MaxSendQueue, sendLimit);
                }
            }
        }
    }

    TEST(ServerConstructorTest, PropagatesBindFailure)
    {
        boost::asio::io_context context;
        boost::asio::ip::tcp::acceptor occupied(context);
        OccupyEndpoint(occupied);
        try
        {
            const Server server(MakeServerOptions(1, occupied.local_endpoint()));
            FAIL() << "Constructing a second listener on an occupied endpoint must fail";
        }
        catch (const boost::system::system_error& error)
        {
            EXPECT_TRUE(error.code() == boost::asio::error::address_in_use
                        || error.code() == boost::asio::error::access_denied)
                << error.what();
            EXPECT_NE(std::string(error.what()).find("Failed to create server acceptor"),
                      std::string::npos);
        }
    }

    TEST(ServerStopTest, RejectsStopBeforeRun)
    {
        Server server(MakeServerOptions(1, LoopbackEndpoint()));
        const auto result = server.Stop();
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), MakeErrorCode(Error::ServerNotStopping));
    }

    TEST(ServerSignalDeathTest, InterruptAllowsExplicitStopAndRejectsRestart)
    {
        ASSERT_EXIT(RunChildWithDeadline([] -> void { SignalLifecycle(SIGINT, true); }),
                    testing::ExitedWithCode(0),
                    "");
    }

    TEST(ServerSignalDeathTest, TerminationAllowsExplicitStopAndRejectsRestart)
    {
        ASSERT_EXIT(RunChildWithDeadline([] -> void { SignalLifecycle(SIGTERM, true); }),
                    testing::ExitedWithCode(0),
                    "");
    }

    TEST(ServerSignalDeathTest, DestructorStopsAfterShutdownRequest)
    {
        ASSERT_EXIT(RunChildWithDeadline([] -> void { SignalLifecycle(SIGTERM, false); }),
                    testing::ExitedWithCode(0),
                    "");
    }

    TEST(ServerSignalDeathTest, SignalBeforeRunDoesNotDeadlockSingleWorker)
    {
        ASSERT_EXIT(RunChildWithDeadline([] -> void { SignalLifecycle(SIGTERM, true, 1); }),
                    testing::ExitedWithCode(0),
                    "");
    }

    TEST(ServerSignalDeathTest, ServesClientAndWaitsForSignalDuringRun)
    {
        ASSERT_EXIT(RunChildWithDeadline([] -> void { RunningServerLifecycle(false); }),
                    testing::ExitedWithCode(0),
                    "");
    }

    TEST(ServerSignalDeathTest, ShutdownClosesActiveClientBeforeJoiningWorkers)
    {
        ASSERT_EXIT(RunChildWithDeadline([] -> void { RunningServerLifecycle(true); }),
                    testing::ExitedWithCode(0),
                    "");
    }

    TEST(ServerSignalDeathTest, StopsRunningServerWithoutSignalAndRejectsRestart)
    {
        ASSERT_EXIT(RunChildWithDeadline([] -> void { RunningServerLifecycle(false, false); }),
                    testing::ExitedWithCode(0),
                    "");
    }

    TEST(ServerSignalDeathTest, StopWithoutSignalClosesActiveClient)
    {
        ASSERT_EXIT(RunChildWithDeadline([] -> void { RunningServerLifecycle(true, false); }),
                    testing::ExitedWithCode(0),
                    "");
    }

    TEST(ServerSignalDeathTest, ConcurrentRunHasExactlyOneWinner)
    {
        ASSERT_EXIT(RunChildWithDeadline([] -> void { ConcurrentLifecycle(true); }),
                    testing::ExitedWithCode(0),
                    "");
    }

    TEST(ServerSignalDeathTest, ConcurrentStopHasExactlyOneWinner)
    {
        ASSERT_EXIT(RunChildWithDeadline([] -> void { ConcurrentLifecycle(false); }),
                    testing::ExitedWithCode(0),
                    "");
    }

}   // namespace Flux::Test
