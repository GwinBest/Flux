#include "Utils/Error.hpp"
#include "gtest/gtest.h"

#include <array>
#include <string_view>

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
        };

        for (const auto& [value, message] : cases)
        {
            SCOPED_TRACE(message);
            const auto code = MakeErrorCode(value);
            EXPECT_TRUE(code);
            EXPECT_EQ(code.value(), static_cast<int>(value));
            EXPECT_EQ(&code.category(), &GetErrorCategory());
            EXPECT_EQ(code.message(), message);
        }
    }

    TEST(ErrorCategoryTest, DescribesUnknownValues)
    {
        for (const auto value :
             {0, static_cast<int>(Error::ConnectionLimitReached) + 1, 127, 255, -1})
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
}   // namespace Flux::Test
