#include "Core/Acceptor.hpp"
#include "gtest/gtest.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include "Core/IoContextPool.hpp"

namespace Flux::Test
{
    using namespace std::chrono_literals;

    namespace
    {
        using Tcp = boost::asio::ip::tcp;

        constexpr std::size_t SingleThreadCount = 1U;
        constexpr unsigned short AnyPort = 0U;
        constexpr std::string_view NotInitializedError = "Acceptor::StartAccepting(): Acceptor "
                                                         "status is not Status::Initialized";

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
            acceptor.close(ignoredError);

            return endpoint;
        }
    }   // namespace

    static_assert(!std::is_copy_constructible_v<Acceptor>);
    static_assert(!std::is_copy_assignable_v<Acceptor>);
    static_assert(!std::is_move_constructible_v<Acceptor>);
    static_assert(!std::is_move_assignable_v<Acceptor>);
    static_assert(std::is_nothrow_destructible_v<Acceptor>);

    TEST(AcceptorFactoryTest, CreatesSharedAcceptorBoundToEndpoint)
    {
        const IoContextPool pool(SingleThreadCount);

        const auto acceptor = Acceptor::CreateAcceptor(pool.GetStrand(), MakeLoopbackEndpoint());

        ASSERT_NE(acceptor, nullptr);
        EXPECT_FALSE(acceptor->StopAccepting());
    }

    TEST(AcceptorStartAcceptingTest, StartsAcceptingOnceFromInitializedState)
    {
        const IoContextPool pool(SingleThreadCount);
        const auto acceptor = Acceptor::CreateAcceptor(pool.GetStrand(), MakeLoopbackEndpoint());

        const auto startResult = acceptor->StartAccepting();
        const auto repeatedStartResult = acceptor->StartAccepting();
        const auto stopError = acceptor->StopAccepting();

        EXPECT_TRUE(startResult.value_or(false));
        EXPECT_FALSE(stopError);
        ASSERT_FALSE(repeatedStartResult.has_value());
        EXPECT_EQ(repeatedStartResult.error(), NotInitializedError);
    }

    TEST(AcceptorStartAcceptingTest, RejectsStartAfterStop)
    {
        const IoContextPool pool(SingleThreadCount);
        const auto acceptor = Acceptor::CreateAcceptor(pool.GetStrand(), MakeLoopbackEndpoint());

        const auto stopError = acceptor->StopAccepting();
        const auto startResult = acceptor->StartAccepting();

        EXPECT_FALSE(stopError);
        ASSERT_FALSE(startResult.has_value());
        EXPECT_EQ(startResult.error(), NotInitializedError);
    }

    TEST(AcceptorStartAcceptingTest, ListeningAcceptorAllowsClientConnection)
    {
        IoContextPool pool(SingleThreadCount);
        const auto endpoint = ReserveLoopbackEndpoint();
        const auto acceptor = Acceptor::CreateAcceptor(pool.GetStrand(), endpoint);
        boost::asio::io_context clientContext;
        Tcp::socket client(clientContext);
        boost::system::error_code connectError;

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.value_or(false));

        const auto startResult = acceptor->StartAccepting();
        EXPECT_TRUE(startResult.value_or(false));

        client.connect(endpoint, connectError);

        boost::system::error_code closeError;
        client.close(closeError);

        const auto stopError = acceptor->StopAccepting();
        const auto stopResult = pool.Stop();

        EXPECT_FALSE(connectError) << connectError.message();
        EXPECT_FALSE(closeError) << closeError.message();
        EXPECT_FALSE(stopError) << stopError.message();
        EXPECT_TRUE(stopResult.value_or(false));
    }

    TEST(AcceptorStopAcceptingTest, ClosesListeningSocket)
    {
        const IoContextPool pool(SingleThreadCount);
        const auto endpoint = ReserveLoopbackEndpoint();
        const auto acceptor = Acceptor::CreateAcceptor(pool.GetStrand(), endpoint);
        boost::asio::io_context clientContext;
        Tcp::socket client(clientContext);
        boost::system::error_code connectError;

        const auto stopError = acceptor->StopAccepting();
        client.connect(endpoint, connectError);

        EXPECT_FALSE(stopError) << stopError.message();
        EXPECT_TRUE(connectError);
    }

    TEST(AcceptorStopAcceptingTest, AllowsRepeatedStop)
    {
        const IoContextPool pool(SingleThreadCount);
        const auto acceptor = Acceptor::CreateAcceptor(pool.GetStrand(), MakeLoopbackEndpoint());

        const auto firstStopError = acceptor->StopAccepting();
        const auto secondStopError = acceptor->StopAccepting();

        EXPECT_FALSE(firstStopError) << firstStopError.message();
        EXPECT_FALSE(secondStopError) << secondStopError.message();
    }
}   // namespace Flux::Test
