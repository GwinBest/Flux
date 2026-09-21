#include <atomic>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

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
            Tcp::acceptor _listener {
                _context,
                {boost::asio::ip::address_v4::loopback(), 0}
            };
            Tcp::socket _peer {_context};
            ConnectionManager _manager;
            std::shared_ptr<Connection> _client;

            void SetUp() override
            {
                _peer.connect(_listener.local_endpoint());
                const auto result = _manager.CreateConnection(_listener.accept());

                ASSERT_TRUE(result.has_value());

                _client = *result;
            }

            void TearDown() override
            {
                EXPECT_TRUE(_manager.CloseAllConnections().has_value());
                _context.restart();
                _context.run_for(100ms);
            }
        };
    }   // namespace

    TEST_F(ConnectionTest, ReportsStateErrorsAsynchronouslyAndPreservesCommandOrder)
    {
        std::vector<Result> results;
        const auto completed = [&](Result result) { results.push_back(result); };

        ASSERT_TRUE(_client->SendAsync({}, completed));
        ASSERT_TRUE(_client->StartAsync(completed));
        ASSERT_TRUE(_client->StartAsync(completed));
        ASSERT_TRUE(_manager.CloseConnection(_client, completed));
        ASSERT_TRUE(_client->SendAsync({}, completed));
        EXPECT_TRUE(results.empty());

        _context.run_for(1s);

        ASSERT_EQ(results.size(), 5U);
        ASSERT_FALSE(results[0]);
        EXPECT_EQ(results[0].error(), MakeErrorCode(Error::ConnectionNotStarted));
        EXPECT_TRUE(results[1]);
        ASSERT_FALSE(results[2]);
        EXPECT_EQ(results[2].error(), MakeErrorCode(Error::ConnectionAlreadyStarted));
        EXPECT_TRUE(results[3]);
        ASSERT_FALSE(results[4]);
        EXPECT_EQ(results[4].error(), MakeErrorCode(Error::ConnectionNotStarted));
    }

    TEST_F(ConnectionTest, PreservesQueuedWriteOrder)
    {
        const std::vector first(128 * 1024, std::byte {0x25});
        const std::vector second(128 * 1024, std::byte {0x72});

        auto expected = first;
        expected.insert(expected.end(), second.begin(), second.end());

        std::vector<std::byte> incoming(expected.size());
        bool completed = false;

        boost::asio::async_read(_peer,
                                boost::asio::buffer(incoming),
                                [&](const ErrorCode& error, const std::size_t count) -> void {
                                    EXPECT_FALSE(error);
                                    EXPECT_EQ(count, incoming.size());
                                    completed = true;
                                });
        ASSERT_TRUE(_client->StartAsync());
        ASSERT_TRUE(_client->SendAsync(first));
        ASSERT_TRUE(_client->SendAsync(second));

        _context.run_for(1s);
        _peer.cancel();
        _context.restart();
        _context.poll();

        EXPECT_TRUE(completed);
        EXPECT_EQ(incoming, expected);
    }

    TEST_F(ConnectionTest, ContinuesReadingUntilPeerDisconnects)
    {
        ASSERT_TRUE(_client->StartAsync());
        const std::vector data(32768, std::byte {1});

        boost::asio::write(_peer, boost::asio::buffer(data));

        _peer.shutdown(Tcp::socket::shutdown_send);
        _context.run_for(1s);

        EXPECT_TRUE(_context.stopped());

        bool completed = false;

        ASSERT_TRUE(_client->SendAsync({}, [&](Result result) -> void {
            completed = true;
            ASSERT_FALSE(result);
            EXPECT_EQ(result.error(), MakeErrorCode(Error::ConnectionNotStarted));
        }));

        _context.restart();
        _context.run_for(1s);

        EXPECT_TRUE(completed);
    }

    TEST_F(ConnectionTest, CloseWithPendingIoKeepsBuffersAliveUntilCancellation)
    {
        ASSERT_TRUE(_client->StartAsync());
        ASSERT_TRUE(_client->SendAsync(std::vector(1024 * 1024, std::byte {1})));
        ASSERT_TRUE(_client->SendAsync({std::byte {2}}));

        const std::weak_ptr weakClient = _client;

        ASSERT_TRUE(_manager.CloseConnection(_client));

        _client.reset();
        _context.run_for(1s);

        EXPECT_TRUE(weakClient.expired());
    }

    TEST_F(ConnectionTest, SerializesCloseAgainstConcurrentSends)
    {
        auto work = boost::asio::make_work_guard(_context);

        ASSERT_TRUE(_client->StartAsync());

        std::atomic completed {0};
        std::atomic failures {0};
        std::vector<std::jthread> workers;

        for (int index = 0; index < 4; ++index)
            workers.emplace_back([&] -> void { _context.run_for(2s); });

        std::vector<std::jthread> senders;

        for (int index = 0; index < 4; ++index)
        {
            senders.emplace_back([&] -> void {
                for (int message = 0; message < 32; ++message)
                {
                    if (!_client->SendAsync({std::byte {1}}, [&](Result result) -> void {
                            if (!result
                                && result.error() != MakeErrorCode(Error::ConnectionNotStarted))
                                ++failures;
                            ++completed;
                        }))
                        ++failures;
                }
            });
        }

        bool closed = false;

        EXPECT_TRUE(_manager.CloseConnection(_client, [&](const Result& result) -> void {
            if (!result)
                ++failures;
            closed = true;
        }));

        senders.clear();
        work.reset();
        workers.clear();

        EXPECT_TRUE(closed);
        EXPECT_EQ(completed.load(), 128);
        EXPECT_EQ(failures.load(), 0);
    }

    TEST_F(ConnectionTest, SerializesConcurrentSendersAndCompletionHandlers)
    {
        ASSERT_TRUE(_client->StartAsync());

        _context.poll();
        _context.restart();

        std::atomic active {0};
        std::atomic completed {0};
        std::atomic failures {0};

        std::vector<std::byte> incoming(128 * 1024);
        bool received = false;

        boost::asio::async_read(_peer,
                                boost::asio::buffer(incoming),
                                [&](const ErrorCode& error, std::size_t count) -> void {
                                    if (error || count != incoming.size())
                                        ++failures;
                                    received = true;
                                });

        std::vector<std::jthread> workers;

        for (int index = 0; index < 4; ++index)
            workers.emplace_back([&] -> void { _context.run_for(2s); });

        std::vector<std::jthread> senders;

        for (int index = 0; index < 4; ++index)
        {
            senders.emplace_back([&] -> void {
                for (int message = 0; message < 32; ++message)
                {
                    const auto result = _client->SendAsync(
                        std::vector(1024, std::byte {0x42}),
                        [&](const Result& queued) -> void {
                            if (active.fetch_add(1) != 0 || !queued)
                                ++failures;

                            std::this_thread::yield();

                            --active;
                            ++completed;
                        });
                    if (!result)
                        ++failures;
                }
            });
        }

        senders.clear();
        workers.clear();

        _peer.cancel();
        _context.restart();
        _context.poll();

        EXPECT_EQ(failures.load(), 0);
        EXPECT_EQ(completed.load(), 128);
        EXPECT_TRUE(received);
        EXPECT_EQ(incoming, std::vector(incoming.size(), std::byte {0x42}));
    }
}   // namespace Flux::Test
