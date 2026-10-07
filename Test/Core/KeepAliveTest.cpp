#include "gtest/gtest.h"

#include <array>
#include <bit>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <limits>
#include <string>

// The helper is in an anonymous namespace. Include its original translation unit
// only in Flux-KeepAliveTests, leaving the library and its public API untouched.
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../../Source/Core/Acceptor.cpp"

namespace Flux::Test
{
    namespace
    {
        using Tcp = boost::asio::ip::tcp;

        constexpr KeepAliveOptions DefaultPolicy {
            .IdleSeconds = 60,
            .IntervalSeconds = 5,
            .ProbeCount = 3,
        };

        auto ReadTcpOption(Tcp::socket& socket, const int option) -> int
        {
#ifdef _WIN32
            std::array<char, sizeof(int)> bytes {};
            auto size = static_cast<int>(bytes.size());
            const auto result = ::getsockopt(socket.native_handle(),
                                             IPPROTO_TCP,
                                             option,
                                             bytes.data(),
                                             &size);
            EXPECT_EQ(result, 0) << "WSA error: " << ::WSAGetLastError();
            EXPECT_EQ(size, static_cast<int>(bytes.size()));
            return std::bit_cast<int>(bytes);
#else
            int value = 0;
            socklen_t size = sizeof(value);
            const auto result = ::getsockopt(socket.native_handle(),
                                             IPPROTO_TCP,
                                             option,
                                             &value,
                                             &size);
            EXPECT_EQ(result, 0) << "errno: " << errno;
            EXPECT_EQ(size, sizeof(value));
            return value;
#endif
        }

        auto ExpectPolicy(Tcp::socket& socket, const KeepAliveOptions& options) -> void
        {
            Tcp::socket::keep_alive enabled;
            socket.get_option(enabled);
            EXPECT_TRUE(enabled.value());
#ifdef __APPLE__
            EXPECT_EQ(ReadTcpOption(socket, TCP_KEEPALIVE), options.IdleSeconds);
#elifdef TCP_KEEPIDLE
            EXPECT_EQ(ReadTcpOption(socket, TCP_KEEPIDLE), options.IdleSeconds);
#endif
#ifdef TCP_KEEPINTVL
            EXPECT_EQ(ReadTcpOption(socket, TCP_KEEPINTVL), options.IntervalSeconds);
#endif
#ifdef TCP_KEEPCNT
            EXPECT_EQ(ReadTcpOption(socket, TCP_KEEPCNT), options.ProbeCount);
#endif
        }

        class KeepAliveTest : public testing::TestWithParam<bool>
        {
        protected:
            boost::asio::io_context _context;
            Tcp::socket _socket {_context};

            auto SetUp() -> void override
            {
                ErrorCode error;
                error = _socket.open(GetParam() ? Tcp::v6() : Tcp::v4(), error);
                if (GetParam() && error == boost::asio::error::address_family_not_supported)
                    GTEST_SKIP() << "IPv6 is not available";
                ASSERT_FALSE(error) << error.message();
                _socket.set_option(Tcp::socket::keep_alive(false));
            }
        };
    }   // namespace

    TEST_P(KeepAliveTest, AppliesDefaultPolicyToNativeSocket)
    {
        const auto error = ConfigureKeepAlive(_socket);
        ASSERT_FALSE(error) << error.message();
        ExpectPolicy(_socket, DefaultPolicy);
    }

    TEST_P(KeepAliveTest, AppliesCustomPolicyToNativeSocket)
    {
        constexpr KeepAliveOptions options {
            .IdleSeconds = 31,
            .IntervalSeconds = 7,
            .ProbeCount = 4,
        };
        const auto error = ConfigureKeepAlive(_socket, options);
        ASSERT_FALSE(error) << error.message();
        ExpectPolicy(_socket, options);
    }

    TEST_P(KeepAliveTest, AcceptsMinimumPositiveValues)
    {
        constexpr KeepAliveOptions options {
            .IdleSeconds = 1,
            .IntervalSeconds = 1,
            .ProbeCount = 1,
        };
        const auto error = ConfigureKeepAlive(_socket, options);
        ASSERT_FALSE(error) << error.message();
        ExpectPolicy(_socket, options);
    }

    TEST_P(KeepAliveTest, ReconfigurationReplacesEveryOption)
    {
        constexpr KeepAliveOptions replacement {
            .IdleSeconds = 42,
            .IntervalSeconds = 8,
            .ProbeCount = 6,
        };
        ASSERT_FALSE(
            ConfigureKeepAlive(_socket,
                               {.IdleSeconds = 31, .IntervalSeconds = 7, .ProbeCount = 4}));
        ASSERT_FALSE(ConfigureKeepAlive(_socket, replacement));
        ExpectPolicy(_socket, replacement);
        ASSERT_FALSE(ConfigureKeepAlive(_socket));
        ExpectPolicy(_socket, DefaultPolicy);
    }

    TEST_P(KeepAliveTest, ReapplyingPolicyReenablesDisabledKeepAlive)
    {
        ASSERT_FALSE(ConfigureKeepAlive(_socket));
        _socket.set_option(Tcp::socket::keep_alive(false));
        ASSERT_FALSE(ConfigureKeepAlive(_socket));
        ExpectPolicy(_socket, DefaultPolicy);
    }

    TEST_P(KeepAliveTest, RejectsEachNonPositiveOptionBeforeChangingSocket)
    {
        constexpr auto minimum = std::numeric_limits<int>::min();
        for (const auto& options : {
                 KeepAliveOptions {.IdleSeconds = 0,       .IntervalSeconds = 5,       .ProbeCount = 3      },
                 KeepAliveOptions {.IdleSeconds = -1,      .IntervalSeconds = 5,       .ProbeCount = 3      },
                 KeepAliveOptions {.IdleSeconds = minimum, .IntervalSeconds = 5,       .ProbeCount = 3      },
                 KeepAliveOptions {.IdleSeconds = 60,      .IntervalSeconds = 0,       .ProbeCount = 3      },
                 KeepAliveOptions {.IdleSeconds = 60,      .IntervalSeconds = -1,      .ProbeCount = 3      },
                 KeepAliveOptions {.IdleSeconds = 60,      .IntervalSeconds = minimum, .ProbeCount = 3      },
                 KeepAliveOptions {.IdleSeconds = 60,      .IntervalSeconds = 5,       .ProbeCount = 0      },
                 KeepAliveOptions {.IdleSeconds = 60,      .IntervalSeconds = 5,       .ProbeCount = -1     },
                 KeepAliveOptions {.IdleSeconds = 60,      .IntervalSeconds = 5,       .ProbeCount = minimum},
        })
        {
            SCOPED_TRACE(testing::Message() << options.IdleSeconds << ',' << options.IntervalSeconds
                                            << ',' << options.ProbeCount);
            EXPECT_EQ(ConfigureKeepAlive(_socket, options), boost::asio::error::invalid_argument);
            Tcp::socket::keep_alive enabled;
            _socket.get_option(enabled);
            EXPECT_FALSE(enabled.value());
        }
    }

    TEST_P(KeepAliveTest, InvalidReconfigurationPreservesExistingPolicy)
    {
        constexpr KeepAliveOptions options {
            .IdleSeconds = 31,
            .IntervalSeconds = 7,
            .ProbeCount = 4,
        };
        ASSERT_FALSE(ConfigureKeepAlive(_socket, options));
        EXPECT_EQ(ConfigureKeepAlive(_socket, {42, 0, 6}), boost::asio::error::invalid_argument);
        ExpectPolicy(_socket, options);
    }

    TEST_P(KeepAliveTest, HonorsNativeProbeCountLimit)
    {
        constexpr KeepAliveOptions options {
            .IdleSeconds = 60,
            .IntervalSeconds = 5,
            .ProbeCount = std::numeric_limits<int>::max(),
        };
#ifdef __APPLE__
        // Darwin accepts TCP_KEEPCNT up to INT32_MAX; INT_MAX is not out of range.
        const auto error = ConfigureKeepAlive(_socket, options);
        ASSERT_FALSE(error) << error.message();
        ExpectPolicy(_socket, options);
#else
        EXPECT_EQ(ConfigureKeepAlive(_socket, options), boost::asio::error::invalid_argument);
#endif
        EXPECT_TRUE(_socket.is_open());
    }

#ifdef __APPLE__
    TEST_P(KeepAliveTest, ReportsNativeErrorForOutOfRangeTimeouts)
    {
        // Darwin limits these seconds values to UINT32_MAX / TCP_RETRANSHZ.
        constexpr auto maximum = std::numeric_limits<int>::max();
        for (const auto& options : {
                 KeepAliveOptions {.IdleSeconds = maximum, .IntervalSeconds = 5,       .ProbeCount = 3},
                 KeepAliveOptions {.IdleSeconds = 60,      .IntervalSeconds = maximum, .ProbeCount = 3},
        })
        {
            SCOPED_TRACE(testing::Message()
                         << options.IdleSeconds << ',' << options.IntervalSeconds);
            EXPECT_EQ(ConfigureKeepAlive(_socket, options), boost::asio::error::invalid_argument);
            EXPECT_TRUE(_socket.is_open());
        }
    }
#endif

#ifdef _WIN32
    TEST_P(KeepAliveTest, AcceptsLargestTimeoutsWithoutMillisecondOverflow)
    {
        constexpr auto maximumSeconds = static_cast<int>(std::numeric_limits<ULONG>::max() / 1000);
        constexpr KeepAliveOptions options {
            .IdleSeconds = maximumSeconds,
            .IntervalSeconds = maximumSeconds,
            .ProbeCount = 3,
        };
        const auto error = ConfigureKeepAlive(_socket, options);
        ASSERT_FALSE(error) << error.message();
        ExpectPolicy(_socket, options);
    }

    TEST_P(KeepAliveTest, OverflowReconfigurationPreservesExistingPolicy)
    {
        constexpr KeepAliveOptions options {
            .IdleSeconds = 31,
            .IntervalSeconds = 7,
            .ProbeCount = 4,
        };
        ASSERT_FALSE(ConfigureKeepAlive(_socket, options));
        constexpr auto overflowSeconds = static_cast<int>(std::numeric_limits<ULONG>::max() / 1000)
                                       + 1;
        for (const auto seconds : {overflowSeconds, std::numeric_limits<int>::max()})
        {
            SCOPED_TRACE(seconds);
            EXPECT_EQ(ConfigureKeepAlive(_socket, {seconds, 5, 3}),
                      boost::asio::error::invalid_argument);
            ExpectPolicy(_socket, options);
            EXPECT_EQ(ConfigureKeepAlive(_socket, {60, seconds, 3}),
                      boost::asio::error::invalid_argument);
            ExpectPolicy(_socket, options);
        }
    }
#endif

    INSTANTIATE_TEST_SUITE_P(IpVersions,
                             KeepAliveTest,
                             testing::Bool(),
                             [](const testing::TestParamInfo<bool>& testCase) -> std::string {
                                 return testCase.param ? "IPv6" : "IPv4";
                             });

    TEST(KeepAliveValidationTest, UnopenedSocketReportsBadDescriptor)
    {
        boost::asio::io_context context;
        Tcp::socket socket(context);
        EXPECT_EQ(ConfigureKeepAlive(socket), boost::asio::error::bad_descriptor);
    }

    TEST(KeepAliveValidationTest, ClosedSocketReportsBadDescriptor)
    {
        boost::asio::io_context context;
        Tcp::socket socket(context, Tcp::v4());
        socket.close();
        EXPECT_EQ(ConfigureKeepAlive(socket), boost::asio::error::bad_descriptor);
    }

    TEST(KeepAliveValidationTest, InvalidOptionsAreReportedBeforeSocketErrors)
    {
        boost::asio::io_context context;
        Tcp::socket socket(context);
        EXPECT_EQ(ConfigureKeepAlive(socket, {0, 5, 3}), boost::asio::error::invalid_argument);
    }

#ifdef _WIN32
    TEST(KeepAliveValidationTest, RejectsWindowsIdleTimeoutMillisecondOverflow)
    {
        boost::asio::io_context context;
        Tcp::socket socket(context, Tcp::v4());
        constexpr auto overflowSeconds = static_cast<int>(std::numeric_limits<ULONG>::max() / 1000)
                                       + 1;
        socket.set_option(Tcp::socket::keep_alive(false));
        for (const auto seconds : {overflowSeconds, std::numeric_limits<int>::max()})
        {
            SCOPED_TRACE(seconds);
            EXPECT_EQ(ConfigureKeepAlive(socket, {seconds, 5, 3}),
                      boost::asio::error::invalid_argument);
            Tcp::socket::keep_alive enabled;
            socket.get_option(enabled);
            EXPECT_FALSE(enabled.value());
        }
    }

    TEST(KeepAliveValidationTest, RejectsWindowsProbeIntervalMillisecondOverflow)
    {
        boost::asio::io_context context;
        Tcp::socket socket(context, Tcp::v4());
        constexpr auto overflowSeconds = static_cast<int>(std::numeric_limits<ULONG>::max() / 1000)
                                       + 1;
        socket.set_option(Tcp::socket::keep_alive(false));
        for (const auto seconds : {overflowSeconds, std::numeric_limits<int>::max()})
        {
            SCOPED_TRACE(seconds);
            EXPECT_EQ(ConfigureKeepAlive(socket, {60, seconds, 3}),
                      boost::asio::error::invalid_argument);
            Tcp::socket::keep_alive enabled;
            socket.get_option(enabled);
            EXPECT_FALSE(enabled.value());
        }
    }
#endif
}   // namespace Flux::Test
