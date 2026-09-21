#include "Server.hpp"
#include "gtest/gtest.h"

#include <boost/asio/error.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <future>
#include <string>
#include <thread>
#include <type_traits>

#include "Support/Socket.hpp"

namespace Flux::Test
{
    namespace
    {
        auto LoopbackEndpoint() -> Endpoint
        {
            return {boost::asio::ip::address_v4::loopback(), 0};
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

        auto SignalLifecycle(const int signalNumber, const bool explicitStop) -> void
        {
            {
                Server server(2, LoopbackEndpoint());
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
            FinishChildAfterShutdown();
        }

        auto RunningServerLifecycle() -> void
        {
            using namespace std::chrono_literals;
            using Tcp = boost::asio::ip::tcp;
            {
                boost::asio::io_context clientContext;
                Tcp::acceptor reservation(clientContext, LoopbackEndpoint());

                const auto endpoint = reservation.local_endpoint();
                reservation.close();

                Server server(1, endpoint);
                auto run = std::async(std::launch::async,
                                      [&server] -> Result { return server.Run(); });

                Tcp::socket client(clientContext);
                client.connect(endpoint);

                if (run.wait_for(0s) != std::future_status::timeout)
                    FailChild("Server run returned before the shutdown signal\n");

                if (const auto prematureStop = server.Stop();
                    prematureStop
                    || prematureStop.error() != MakeErrorCode(Error::ServerNotStopping))
                    FailChild("Premature stop returned an unexpected result\n");

                if (std::raise(SIGTERM) != 0)
                    FailChild("Failed to raise shutdown signal\n");

                if (run.wait_for(3s) != std::future_status::ready || !run.get())
                    FailChild("Server run did not complete successfully after the signal\n");

                if (!server.Stop())
                    FailChild("Server stop failed\n");

                client.non_blocking(true);

                char byte = '\0';
                ErrorCode readError;

                if (const auto bytes = client.read_some(boost::asio::buffer(&byte, 1), readError);
                    bytes != 0
                    || (readError != boost::asio::error::eof
                        && readError != boost::asio::error::connection_reset))
                    FailChild("Client remained connected after server shutdown\n");
            }
            FinishChildAfterShutdown();
        }
    }   // namespace

    static_assert(!std::is_copy_constructible_v<Server>);
    static_assert(!std::is_copy_assignable_v<Server>);
    static_assert(!std::is_move_constructible_v<Server>);
    static_assert(!std::is_move_assignable_v<Server>);
    static_assert(std::is_destructible_v<Server>);

    TEST(ServerConstructorTest, NormalizesZeroThreadCount)
    {
        const Server server(0, LoopbackEndpoint());
        EXPECT_EQ(server.GetThreadCount(), 1U);
    }

    TEST(ServerConstructorTest, PreservesPositiveThreadCount)
    {
        const Server server(3, LoopbackEndpoint());
        EXPECT_EQ(server.GetThreadCount(), 3U);
    }

    TEST(ServerConstructorTest, PropagatesBindFailure)
    {
        boost::asio::io_context context;
        boost::asio::ip::tcp::acceptor occupied(context);
        OccupyEndpoint(occupied);

        try
        {
            const Server server(1, occupied.local_endpoint());
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

    TEST(ServerStopTest, RejectsStopWithoutShutdownRequest)
    {
        Server server(1, LoopbackEndpoint());
        const auto result = server.Stop();

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), MakeErrorCode(Error::ServerNotStopping));
    }

    TEST(ServerSignalDeathTest, InterruptAllowsExplicitStopAndRejectsRestart)
    {
        ASSERT_EXIT(SignalLifecycle(SIGINT, true), testing::ExitedWithCode(0), "");
    }

    TEST(ServerSignalDeathTest, TerminationAllowsExplicitStopAndRejectsRestart)
    {
        ASSERT_EXIT(SignalLifecycle(SIGTERM, true), testing::ExitedWithCode(0), "");
    }

    TEST(ServerSignalDeathTest, DestructorStopsAfterShutdownRequest)
    {
        ASSERT_EXIT(SignalLifecycle(SIGTERM, false), testing::ExitedWithCode(0), "");
    }

    TEST(ServerSignalDeathTest, ServesClientAndWaitsForSignalDuringRun)
    {
        ASSERT_EXIT(RunningServerLifecycle(), testing::ExitedWithCode(0), "");
    }

}   // namespace Flux::Test
