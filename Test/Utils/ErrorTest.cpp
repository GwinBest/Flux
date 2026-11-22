#include "Utils/Error.hpp"

#include "gtest/gtest.h"

#include <array>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Flux::Test
{
    TEST(ErrorCategoryTest, HasStableIdentityAndName)
    {
        EXPECT_EQ(&GetErrorCategory(), &GetErrorCategory());
        EXPECT_STREQ(GetErrorCategory().name(), "flux");
        EXPECT_NE(&GetErrorCategory(), &boost::system::generic_category());
    }

    TEST(ErrorCategoryTest, DescribesEveryDeclaredError)
    {
        struct ErrorCase
        {
            Error Value;
            std::string_view Message;
        };

        constexpr std::array cases {
            ErrorCase {
                       .Value = Error::ConnectionAlreadyStarted,
                       .Message = "connection is already started",
                       },
            ErrorCase {
                       .Value = Error::ConnectionNotStarted,
                       .Message = "connection is not started",
                       },
            ErrorCase {.Value = Error::ConnectionClosed, .Message = "connection is closed"},
            ErrorCase {
                       .Value = Error::ConnectionException,
                       .Message = "connection operation threw an exception",
                       },
            ErrorCase {
                       .Value = Error::ConnectionLimitReached,
                       .Message = "connection limit reached",
                       },
            ErrorCase {.Value = Error::SendQueueFull, .Message = "send queue full"},
            ErrorCase {
                       .Value = Error::ServerNotInitialized,
                       .Message = "server is not initialized",
                       },
            ErrorCase {.Value = Error::ServerNotRunning, .Message = "server is not running"},
            ErrorCase {.Value = Error::ServerNotStopping, .Message = "server is not stopping"},
            ErrorCase {
                       .Value = Error::IoContextPoolNotStarted,
                       .Message = "IO context pool is not started",
                       },
            ErrorCase {
                       .Value = Error::IoContextPoolNotRunning,
                       .Message = "IO context pool is not running",
                       },
            ErrorCase {
                       .Value = Error::IoContextPoolStopFromWorkerThread,
                       .Message = "IO context pool cannot be stopped from one of its worker threads",
                       },
            ErrorCase {
                       .Value = Error::AcceptorNotInitialized,
                       .Message = "acceptor is not initialized",
                       },
            ErrorCase {.Value = Error::AcceptorNotStarted, .Message = "acceptor is not started"},
            ErrorCase {.Value = Error::AcceptorAlreadyOpen, .Message = "acceptor already open"},
            ErrorCase {
                       .Value = Error::IoContextPoolRunFromWorkerThread,
                       .Message = "IO context pool cannot be started from one of its worker threads",
                       },
            ErrorCase {.Value = Error::NoSendCallback, .Message = "no send callback"},
            ErrorCase {.Value = Error::InvalidArgument, .Message = "invalid argument"},
            ErrorCase {.Value = Error::ResultOutOfRange, .Message = "result out of range"},
            ErrorCase {.Value = Error::IoError, .Message = "I/O error"},
            ErrorCase {
                       .Value = Error::StateNotRecoverable,
                       .Message = "state not recoverable",
                       },
            ErrorCase {
                       .Value = Error::FunctionNotSupported,
                       .Message = "function not supported",
                       },
        };

        for (const auto& [value, message] : cases)
        {
            SCOPED_TRACE(message);
            const auto code = MakeErrorCode(value);
            EXPECT_TRUE(code);
            EXPECT_EQ(code.value(), std::to_underlying(value));
            EXPECT_EQ(&code.category(), &GetErrorCategory());
            EXPECT_EQ(code.message(), message);
        }
    }

    TEST(ErrorCategoryTest, DescribesUnknownValues)
    {
        for (const auto value : {
                 0,
                 -1,
                 std::numeric_limits<int>::min(),
                 std::numeric_limits<int>::max(),
             })
        {
            SCOPED_TRACE(value);
            EXPECT_EQ(GetErrorCategory().message(value), "unknown error");
        }
    }

    TEST(ErrorCategoryTest, EqualNumericValuesFromOtherCategoriesAreDistinct)
    {
        const auto code = MakeErrorCode(Error::ConnectionLimitReached);
        EXPECT_NE(code, ErrorCode(code.value(), boost::system::generic_category()));
        EXPECT_NE(code, ErrorCode(code.value(), boost::system::system_category()));
        EXPECT_NE(code, MakeErrorCode(Error::ConnectionClosed));
    }

    TEST(ErrorCategoryTest, EnumUnderlyingTypeIsUint8)
    {
        static_assert(std::is_same_v<std::underlying_type_t<Error>, std::uint8_t>);
    }

    TEST(ErrorCategoryTest, EnumValuesAreSequentialFrom1)
    {
        // ServerNotInitialized = 1, then sequential
        EXPECT_EQ(std::to_underlying(Error::ServerNotInitialized), 1);
        EXPECT_EQ(std::to_underlying(Error::ServerNotRunning), 2);
        EXPECT_EQ(std::to_underlying(Error::ServerNotStopping), 3);
        EXPECT_EQ(std::to_underlying(Error::IoContextPoolNotStarted), 4);
        EXPECT_EQ(std::to_underlying(Error::IoContextPoolNotRunning), 5);
        EXPECT_EQ(std::to_underlying(Error::IoContextPoolStopFromWorkerThread), 6);
        EXPECT_EQ(std::to_underlying(Error::AcceptorNotInitialized), 7);
        EXPECT_EQ(std::to_underlying(Error::AcceptorNotStarted), 8);
        EXPECT_EQ(std::to_underlying(Error::AcceptorAlreadyOpen), 9);
        EXPECT_EQ(std::to_underlying(Error::IoContextPoolRunFromWorkerThread), 10);
        EXPECT_EQ(std::to_underlying(Error::ConnectionAlreadyStarted), 11);
        EXPECT_EQ(std::to_underlying(Error::ConnectionNotStarted), 12);
        EXPECT_EQ(std::to_underlying(Error::ConnectionClosed), 13);
        EXPECT_EQ(std::to_underlying(Error::ConnectionException), 14);
        EXPECT_EQ(std::to_underlying(Error::ConnectionLimitReached), 15);
        EXPECT_EQ(std::to_underlying(Error::SendQueueFull), 16);
        EXPECT_EQ(std::to_underlying(Error::NoSendCallback), 17);
        EXPECT_EQ(std::to_underlying(Error::InvalidArgument), 18);
        EXPECT_EQ(std::to_underlying(Error::ResultOutOfRange), 19);
        EXPECT_EQ(std::to_underlying(Error::IoError), 20);
        EXPECT_EQ(std::to_underlying(Error::StateNotRecoverable), 21);
        EXPECT_EQ(std::to_underlying(Error::FunctionNotSupported), 22);
    }

    TEST(ErrorCategoryTest, MakeErrorCodeReturnsValidCode)
    {
        const auto code = MakeErrorCode(Error::ServerNotInitialized);
        EXPECT_TRUE(code);
        EXPECT_EQ(code.value(), 1);
        EXPECT_EQ(&code.category(), &GetErrorCategory());
    }

    TEST(ErrorCategoryTest, MakeErrorCodeForLastErrorValue)
    {
        const auto code = MakeErrorCode(Error::FunctionNotSupported);
        EXPECT_TRUE(code);
        EXPECT_EQ(code.value(), 22);
        EXPECT_EQ(&code.category(), &GetErrorCategory());
    }

    TEST(ErrorCategoryTest, MakeErrorCodeDifferentValuesAreDistinct)
    {
        constexpr auto value = 22;
        for (int i = 1; i <= value; ++i)
        {
            for (int j = i + 1; j <= value; ++j)
            {
                const auto codeI = MakeErrorCode(static_cast<Error>(i));
                const auto codeJ = MakeErrorCode(static_cast<Error>(j));
                EXPECT_NE(codeI, codeJ) << "i=" << i << " j=" << j;
            }
        }
    }

    TEST(ErrorCategoryTest, MakeErrorCodeIdempotent)
    {
        const auto code1 = MakeErrorCode(Error::ConnectionClosed);
        const auto code2 = MakeErrorCode(Error::ConnectionClosed);
        EXPECT_EQ(code1, code2);
    }

    TEST(ErrorCategoryTest, ErrorCodeTypeAlias)
    {
        static_assert(std::is_same_v<ErrorCode, boost::system::error_code>);
    }

    TEST(ErrorCategoryTest, ResultTypeAlias)
    {
        static_assert(std::is_same_v<Result, std::expected<void, ErrorCode>>);
    }

    TEST(ErrorCategoryTest, ResultSuccessCase)
    {
        constexpr Result result = {};
        EXPECT_TRUE(result.has_value());
        // std::expected::error() asserts when has_value() is true, so we only check has_value()
    }

    TEST(ErrorCategoryTest, ResultErrorCase)
    {
        Result result = std::unexpected(MakeErrorCode(Error::ServerNotRunning));
        EXPECT_FALSE(result.has_value());
        EXPECT_TRUE(result.error());
        EXPECT_EQ(result.error().value(), std::to_underlying(Error::ServerNotRunning));
        EXPECT_EQ(result.error().message(), "server is not running");
    }

    TEST(ErrorCategoryTest, ResultErrorCaseFirstError)
    {
        Result result = std::unexpected(MakeErrorCode(Error::ServerNotInitialized));
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error().value(), 1);
    }

    TEST(ErrorCategoryTest, ResultErrorCaseLastError)
    {
        Result result = std::unexpected(MakeErrorCode(Error::FunctionNotSupported));
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error().value(), 22);
    }

    TEST(ErrorCategoryTest, ResultMoveAssignment)
    {
        Result result1 = std::unexpected(MakeErrorCode(Error::ConnectionClosed));
        Result result2 = std::move(result1);
        EXPECT_FALSE(result2.has_value());
        EXPECT_EQ(result2.error().value(), std::to_underlying(Error::ConnectionClosed));
    }

    TEST(ErrorCategoryTest, ResultCopyAssignment)
    {
        const Result result1 = std::unexpected(MakeErrorCode(Error::SendQueueFull));
        Result result2 = result1;
        EXPECT_FALSE(result2.has_value());
        EXPECT_EQ(result2.error().value(), std::to_underlying(Error::SendQueueFull));
    }

    TEST(ErrorCategoryTest, GetErrorCategoryIsConstCorrect)
    {
        const auto& cat1 = GetErrorCategory();
        const auto& cat2 = GetErrorCategory();
        EXPECT_EQ(&cat1, &cat2);
        EXPECT_STREQ(cat1.name(), "flux");
    }

    TEST(ErrorCategoryTest, MessageDoesNotThrow)
    {
        constexpr auto value = 22;
        for (int i = 1; i <= value; ++i)
        {
            const auto msg = GetErrorCategory().message(i);
            EXPECT_FALSE(msg.empty()) << "message for value " << i << " is empty";
        }
    }

    TEST(ErrorCategoryTest, MessageForZeroReturnsUnknown)
    {
        EXPECT_EQ(GetErrorCategory().message(0), "unknown error");
    }

    TEST(ErrorCategoryTest, MessageForNegativeReturnsUnknown)
    {
        EXPECT_EQ(GetErrorCategory().message(-1), "unknown error");
    }

    TEST(ErrorCategoryTest, MessageForBeyondMaxEnumReturnsUnknown)
    {
        // 23 is beyond the last enum value (22)
        EXPECT_EQ(GetErrorCategory().message(23), "unknown error");
        EXPECT_EQ(GetErrorCategory().message(255), "unknown error");
        EXPECT_EQ(GetErrorCategory().message(std::numeric_limits<int>::max()), "unknown error");
    }

    TEST(ErrorCategoryTest, AllMessagesAreUnique)
    {
        // Verify that no two error codes share the same message
        struct Entry
        {
            int Value;
            std::string Message;
        };

        std::vector<Entry> entries;
        constexpr auto value = 22;
        for (int i = 1; i <= value; ++i)
        {
            entries.push_back(Entry {.Value = i, .Message = GetErrorCategory().message(i)});
        }

        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            for (std::size_t j = i + 1; j < entries.size(); ++j)
            {
                EXPECT_NE(entries.at(i).Message, entries.at(j).Message)
                    << "Duplicate message for values " << entries.at(i).Value << " and "
                    << entries.at(j).Value << ": " << entries.at(i).Message;
            }
        }
    }

    TEST(ErrorCategoryTest, AllMessagesAreNonEmpty)
    {
        constexpr auto value = 22;
        for (int i = 1; i <= value; ++i)
        {
            const auto msg = GetErrorCategory().message(i);
            EXPECT_FALSE(msg.empty()) << "Empty message for error value " << i;
            EXPECT_NE(msg, "unknown error") << "Error value " << i << " returned unknown error";
        }
    }
}   // namespace Flux::Test
