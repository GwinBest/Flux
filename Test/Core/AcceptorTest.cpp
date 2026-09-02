#include "Core/Acceptor.hpp"
#include "gtest/gtest.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <expected>
#include <string>
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

        template <typename TValue>
        auto ErrorMessage(const std::expected<TValue, ErrorCode>& result) -> std::string
        {
            return result.has_value() ? std::string{} : result.error().message();
        }

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

        const auto acceptorResult =
            Acceptor::CreateAcceptor(pool.GetStrand(), MakeLoopbackEndpoint());

        ASSERT_TRUE(acceptorResult.has_value()) << ErrorMessage(acceptorResult);
        const auto acceptor = *acceptorResult;
        ASSERT_NE(acceptor, nullptr);
        const auto stopResult = acceptor->StopAccepting();
        EXPECT_TRUE(stopResult.has_value()) << ErrorMessage(stopResult);
    }

    TEST(AcceptorStartAcceptingTest, StartsAcceptingOnceFromInitializedState)
    {
        const IoContextPool pool(SingleThreadCount);
        const auto acceptorResult =
            Acceptor::CreateAcceptor(pool.GetStrand(), MakeLoopbackEndpoint());
        ASSERT_TRUE(acceptorResult.has_value()) << ErrorMessage(acceptorResult);
        const auto acceptor = *acceptorResult;

        const auto startResult = acceptor->StartAccepting();
        const auto repeatedStartResult = acceptor->StartAccepting();
        const auto stopError = acceptor->StopAccepting();

        EXPECT_TRUE(startResult.has_value()) << ErrorMessage(startResult);
        EXPECT_TRUE(stopError.has_value()) << ErrorMessage(stopError);
        ASSERT_FALSE(repeatedStartResult.has_value());
        EXPECT_EQ(repeatedStartResult.error(), MakeErrorCode(Error::AcceptorNotInitialized));
    }

    TEST(AcceptorStartAcceptingTest, RejectsStartAfterStop)
    {
        const IoContextPool pool(SingleThreadCount);
        const auto acceptorResult =
            Acceptor::CreateAcceptor(pool.GetStrand(), MakeLoopbackEndpoint());
        ASSERT_TRUE(acceptorResult.has_value()) << ErrorMessage(acceptorResult);
        const auto acceptor = *acceptorResult;

        const auto stopError = acceptor->StopAccepting();
        const auto startResult = acceptor->StartAccepting();

        EXPECT_TRUE(stopError.has_value()) << ErrorMessage(stopError);
        ASSERT_FALSE(startResult.has_value());
        EXPECT_EQ(startResult.error(), MakeErrorCode(Error::AcceptorNotInitialized));
    }

    TEST(AcceptorStartAcceptingTest, ListeningAcceptorAllowsClientConnection)
    {
        IoContextPool pool(SingleThreadCount);
        const auto endpoint = ReserveLoopbackEndpoint();
        const auto acceptorResult = Acceptor::CreateAcceptor(pool.GetStrand(), endpoint);
        ASSERT_TRUE(acceptorResult.has_value()) << ErrorMessage(acceptorResult);
        const auto acceptor = *acceptorResult;
        boost::asio::io_context clientContext;
        Tcp::socket client(clientContext);
        boost::system::error_code connectError;

        const auto runResult = pool.Run();
        EXPECT_TRUE(runResult.has_value()) << ErrorMessage(runResult);

        const auto startResult = acceptor->StartAccepting();
        EXPECT_TRUE(startResult.has_value()) << ErrorMessage(startResult);

        client.connect(endpoint, connectError);

        boost::system::error_code closeError;
        client.close(closeError);

        const auto stopError = acceptor->StopAccepting();
        const auto stopResult = pool.Stop();

        EXPECT_FALSE(connectError) << connectError.message();
        EXPECT_FALSE(closeError) << closeError.message();
        EXPECT_TRUE(stopError.has_value()) << ErrorMessage(stopError);
        EXPECT_TRUE(stopResult.has_value()) << ErrorMessage(stopResult);
    }

    TEST(AcceptorStopAcceptingTest, ClosesListeningSocket)
    {
        const IoContextPool pool(SingleThreadCount);
        const auto endpoint = ReserveLoopbackEndpoint();
        const auto acceptorResult = Acceptor::CreateAcceptor(pool.GetStrand(), endpoint);
        ASSERT_TRUE(acceptorResult.has_value()) << ErrorMessage(acceptorResult);
        const auto acceptor = *acceptorResult;
        boost::asio::io_context clientContext;
        Tcp::socket client(clientContext);
        boost::system::error_code connectError;

        const auto stopError = acceptor->StopAccepting();
        client.connect(endpoint, connectError);

        EXPECT_TRUE(stopError.has_value()) << ErrorMessage(stopError);
        EXPECT_TRUE(connectError);
    }

    TEST(AcceptorStopAcceptingTest, AllowsRepeatedStop)
    {
        const IoContextPool pool(SingleThreadCount);
        const auto acceptorResult =
            Acceptor::CreateAcceptor(pool.GetStrand(), MakeLoopbackEndpoint());
        ASSERT_TRUE(acceptorResult.has_value()) << ErrorMessage(acceptorResult);
        const auto acceptor = *acceptorResult;

        const auto firstStopError = acceptor->StopAccepting();
        const auto secondStopError = acceptor->StopAccepting();

        EXPECT_TRUE(firstStopError.has_value()) << ErrorMessage(firstStopError);
        EXPECT_TRUE(secondStopError.has_value()) << ErrorMessage(secondStopError);
    }
}   // namespace Flux::Test
