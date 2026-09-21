#include "Core/ConnectionManager.hpp"
#include "gtest/gtest.h"

#include <boost/asio/io_context.hpp>
#include <chrono>
#include <memory>
#include <vector>

namespace Flux::Test
{
    namespace
    {
        using Tcp = boost::asio::ip::tcp;
        using namespace std::chrono_literals;

        class ConnectionManagerTest : public testing::Test
        {
        protected:
            boost::asio::io_context _context;

            Tcp::acceptor _listener {
                _context,
                {boost::asio::ip::address_v4::loopback(), 0}
            };

            std::vector<Tcp::socket> _peers;

            ConnectionManager _manager;

            std::shared_ptr<std::vector<Result>>
                _completions = std::make_shared<std::vector<Result>>();

            auto ConnectedSocket() -> Tcp::socket
            {
                _peers.emplace_back(_context);
                _peers.back().connect(_listener.local_endpoint());
                return _listener.accept();
            }

            auto RecordCompletion() -> Connection::CompletionHandler
            {
                return [results = _completions](Result result) -> void {
                    results->push_back(std::move(result));
                };
            }

            auto Poll() -> void
            {
                _context.restart();
                _context.poll();
            }

            static auto ExpectPeerClosed(Tcp::socket& peer) -> void
            {
                peer.non_blocking(true);
                char byte {};
                ErrorCode error;
                EXPECT_EQ(peer.read_some(boost::asio::buffer(&byte, 1), error), 0U);
                EXPECT_TRUE(error == boost::asio::error::eof
                            || error == boost::asio::error::connection_reset)
                    << error.message();
            }

            void TearDown() override
            {
                EXPECT_TRUE(_manager.CloseAllConnections());
                _context.restart();
                _context.run_for(1s);
                EXPECT_TRUE(_context.stopped());
            }
        };
    }   // namespace

    TEST_F(ConnectionManagerTest, CreateOwnsConnectionWithoutStartingIt)
    {
        auto result = _manager.CreateConnection(ConnectedSocket());

        ASSERT_TRUE(result);
        ASSERT_NE(*result, nullptr);
        ASSERT_TRUE((*result)->SendAsync({}, RecordCompletion()));

        Poll();

        ASSERT_EQ(_completions->size(), 1U);
        ASSERT_FALSE(_completions->front());
        EXPECT_EQ(_completions->front().error(), MakeErrorCode(Error::ConnectionNotStarted));

        const std::weak_ptr weakConnection = *result;
        result->reset();

        EXPECT_FALSE(weakConnection.expired());
        ASSERT_TRUE(_manager.CloseAllConnections());

        Poll();

        EXPECT_TRUE(weakConnection.expired());
        ExpectPeerClosed(_peers.front());
    }

    TEST_F(ConnectionManagerTest, CreateAndStartReportsCompletionAndAllowsSending)
    {
        const auto result = _manager.CreateAndStartConnection(ConnectedSocket(),
                                                              RecordCompletion());
        ASSERT_TRUE(result);
        EXPECT_TRUE(_completions->empty());
        ASSERT_TRUE((*result)->SendAsync({}, RecordCompletion()));

        Poll();

        ASSERT_EQ(_completions->size(), 2U);
        EXPECT_TRUE((*_completions)[0]);
        EXPECT_TRUE((*_completions)[1]);
    }

    TEST_F(ConnectionManagerTest, CreateAndStartReportsInvalidSocketThroughCallback)
    {
        const auto result = _manager.CreateAndStartConnection(Tcp::socket(_context),
                                                              RecordCompletion());

        ASSERT_TRUE(result);
        EXPECT_TRUE(_completions->empty());

        Poll();

        ASSERT_EQ(_completions->size(), 1U);
        ASSERT_FALSE(_completions->front());
        EXPECT_EQ(_completions->front().error(), boost::asio::error::bad_descriptor);
        EXPECT_TRUE(_manager.CloseConnection(*result));
    }

    TEST_F(ConnectionManagerTest, RejectsNullConnectionWithoutCallingCompletion)
    {
        const auto result = _manager.CloseConnection(nullptr, RecordCompletion());

        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), MakeErrorCode(Error::ConnectionNotFound));

        Poll();

        EXPECT_TRUE(_completions->empty());
    }

    TEST_F(ConnectionManagerTest, RejectsConnectionOwnedByAnotherManager)
    {
        const auto connection = _manager.CreateConnection(ConnectedSocket());

        ASSERT_TRUE(connection);

        ConnectionManager otherManager;
        const auto result = otherManager.CloseConnection(*connection, RecordCompletion());

        ASSERT_FALSE(result);
        EXPECT_EQ(result.error(), MakeErrorCode(Error::ConnectionNotFound));

        Poll();

        EXPECT_TRUE(_completions->empty());
        EXPECT_TRUE(_manager.CloseConnection(*connection));
    }

    TEST_F(ConnectionManagerTest, CloseRemovesOnlyRequestedConnectionAndCompletesAsynchronously)
    {
        const auto first = _manager.CreateAndStartConnection(ConnectedSocket());
        const auto second = _manager.CreateAndStartConnection(ConnectedSocket());

        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        EXPECT_NE(*first, *second);
        ASSERT_TRUE(_manager.CloseConnection(*first, RecordCompletion()));
        EXPECT_TRUE(_completions->empty());

        const auto repeated = _manager.CloseConnection(*first, RecordCompletion());

        ASSERT_FALSE(repeated);
        EXPECT_EQ(repeated.error(), MakeErrorCode(Error::ConnectionNotFound));
        ASSERT_TRUE((*second)->SendAsync({}, RecordCompletion()));

        Poll();

        ASSERT_EQ(_completions->size(), 2U);

        for (const auto& result : *_completions) EXPECT_TRUE(result);

        ExpectPeerClosed(_peers[0]);
        _peers[1].non_blocking(true);

        char byte {};
        ErrorCode error;

        EXPECT_EQ(_peers[1].read_some(boost::asio::buffer(&byte, 1), error), 0U);
        EXPECT_TRUE(error == boost::asio::error::would_block
                    || error == boost::asio::error::try_again)
            << error.message();
        EXPECT_TRUE(_manager.CloseConnection(*second));
    }

    TEST_F(ConnectionManagerTest, CloseAllClosesStartedAndUnstartedConnectionsAndReleasesOwnership)
    {
        std::vector<std::weak_ptr<Connection>> connections;
        for (int index = 0; index < 3; ++index)
        {
            auto result = index == 0 ? _manager.CreateConnection(ConnectedSocket())
                                     : _manager.CreateAndStartConnection(ConnectedSocket());
            ASSERT_TRUE(result);
            connections.emplace_back(*result);
        }

        ASSERT_TRUE(_manager.CloseAllConnections());

        _context.restart();
        _context.run_for(1s);

        for (const auto& connection : connections) EXPECT_TRUE(connection.expired());

        for (auto& peer : _peers) ExpectPeerClosed(peer);

        EXPECT_TRUE(_manager.CloseAllConnections());
    }

    TEST_F(ConnectionManagerTest, EmptyManagerCanCloseAllAndCreateNewConnectionsAfterwards)
    {
        ASSERT_TRUE(_manager.CloseAllConnections());

        const auto first = _manager.CreateConnection(ConnectedSocket());

        ASSERT_TRUE(first);
        ASSERT_TRUE(_manager.CloseAllConnections());

        const auto second = _manager.CreateAndStartConnection(ConnectedSocket(),
                                                              RecordCompletion());

        ASSERT_TRUE(second);
        EXPECT_NE(*first, *second);

        Poll();
        
        ASSERT_EQ(_completions->size(), 1U);
        EXPECT_TRUE(_completions->front());
        EXPECT_TRUE(_manager.CloseConnection(*second));
    }
}   // namespace Flux::Test
