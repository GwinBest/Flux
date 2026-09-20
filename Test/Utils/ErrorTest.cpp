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
        for (const auto value : {0, 11, 127, 255, -1})
        {
            SCOPED_TRACE(value);
            EXPECT_EQ(GetErrorCategory().message(value), "unknown error");
        }
    }
}   // namespace Flux::Test
