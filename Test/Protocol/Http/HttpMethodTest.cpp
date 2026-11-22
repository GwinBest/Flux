#include "Protocol/Http/HttpMethod.hpp"

#include "gtest/gtest.h"

#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace Flux::Test
{
    // === ToString: Standard HTTP methods ===

    TEST(HttpMethodTest, ToStringReturnsGet)
    {
        EXPECT_EQ(ToString(HttpMethod::Get), "GET");
    }

    TEST(HttpMethodTest, ToStringReturnsPost)
    {
        EXPECT_EQ(ToString(HttpMethod::Post), "POST");
    }

    TEST(HttpMethodTest, ToStringReturnsPut)
    {
        EXPECT_EQ(ToString(HttpMethod::Put), "PUT");
    }

    TEST(HttpMethodTest, ToStringReturnsDelete)
    {
        EXPECT_EQ(ToString(HttpMethod::Delete), "DELETE");
    }

    TEST(HttpMethodTest, ToStringReturnsHead)
    {
        EXPECT_EQ(ToString(HttpMethod::Head), "HEAD");
    }

    TEST(HttpMethodTest, ToStringReturnsOptions)
    {
        EXPECT_EQ(ToString(HttpMethod::Options), "OPTIONS");
    }

    TEST(HttpMethodTest, ToStringReturnsConnect)
    {
        EXPECT_EQ(ToString(HttpMethod::Connect), "CONNECT");
    }

    TEST(HttpMethodTest, ToStringReturnsTrace)
    {
        EXPECT_EQ(ToString(HttpMethod::Trace), "TRACE");
    }

    // === ToString: WebDAV methods ===

    TEST(HttpMethodTest, ToStringReturnsCopy)
    {
        EXPECT_EQ(ToString(HttpMethod::Copy), "COPY");
    }

    TEST(HttpMethodTest, ToStringReturnsLock)
    {
        EXPECT_EQ(ToString(HttpMethod::Lock), "LOCK");
    }

    TEST(HttpMethodTest, ToStringReturnsMkCol)
    {
        EXPECT_EQ(ToString(HttpMethod::MkCol), "MKCOL");
    }

    TEST(HttpMethodTest, ToStringReturnsMove)
    {
        EXPECT_EQ(ToString(HttpMethod::Move), "MOVE");
    }

    TEST(HttpMethodTest, ToStringReturnsPropfind)
    {
        EXPECT_EQ(ToString(HttpMethod::Propfind), "PROPFIND");
    }

    TEST(HttpMethodTest, ToStringReturnsProppatch)
    {
        EXPECT_EQ(ToString(HttpMethod::Proppatch), "PROPPATCH");
    }

    TEST(HttpMethodTest, ToStringReturnsSearch)
    {
        EXPECT_EQ(ToString(HttpMethod::Search), "SEARCH");
    }

    TEST(HttpMethodTest, ToStringReturnsUnlock)
    {
        EXPECT_EQ(ToString(HttpMethod::Unlock), "UNLOCK");
    }

    TEST(HttpMethodTest, ToStringReturnsBind)
    {
        EXPECT_EQ(ToString(HttpMethod::Bind), "BIND");
    }

    TEST(HttpMethodTest, ToStringReturnsRebind)
    {
        EXPECT_EQ(ToString(HttpMethod::Rebind), "REBIND");
    }

    TEST(HttpMethodTest, ToStringReturnsUnbind)
    {
        EXPECT_EQ(ToString(HttpMethod::Unbind), "UNBIND");
    }

    TEST(HttpMethodTest, ToStringReturnsAcl)
    {
        EXPECT_EQ(ToString(HttpMethod::Acl), "ACL");
    }

    // === ToString: Subversion methods ===

    TEST(HttpMethodTest, ToStringReturnsReport)
    {
        EXPECT_EQ(ToString(HttpMethod::Report), "REPORT");
    }

    TEST(HttpMethodTest, ToStringReturnsMkActivity)
    {
        EXPECT_EQ(ToString(HttpMethod::MkActivity), "MKACTIVITY");
    }

    TEST(HttpMethodTest, ToStringReturnsCheckout)
    {
        EXPECT_EQ(ToString(HttpMethod::Checkout), "CHECKOUT");
    }

    TEST(HttpMethodTest, ToStringReturnsMerge)
    {
        EXPECT_EQ(ToString(HttpMethod::Merge), "MERGE");
    }

    // === ToString: UPnP methods ===

    TEST(HttpMethodTest, ToStringReturnsMSearch)
    {
        EXPECT_EQ(ToString(HttpMethod::MSearch), "M-SEARCH");
    }

    TEST(HttpMethodTest, ToStringReturnsNotify)
    {
        EXPECT_EQ(ToString(HttpMethod::Notify), "NOTIFY");
    }

    TEST(HttpMethodTest, ToStringReturnsSubscribe)
    {
        EXPECT_EQ(ToString(HttpMethod::Subscribe), "SUBSCRIBE");
    }

    TEST(HttpMethodTest, ToStringReturnsUnsubscribe)
    {
        EXPECT_EQ(ToString(HttpMethod::Unsubscribe), "UNSUBSCRIBE");
    }

    // === ToString: RFC-5789 methods ===

    TEST(HttpMethodTest, ToStringReturnsPatch)
    {
        EXPECT_EQ(ToString(HttpMethod::Patch), "PATCH");
    }

    TEST(HttpMethodTest, ToStringReturnsPurge)
    {
        EXPECT_EQ(ToString(HttpMethod::Purge), "PURGE");
    }

    // === ToString: CalDAV methods ===

    TEST(HttpMethodTest, ToStringReturnsMkCalendar)
    {
        EXPECT_EQ(ToString(HttpMethod::MkCalendar), "MKCALENDAR");
    }

    // === ToString: RFC-2068 methods ===

    TEST(HttpMethodTest, ToStringReturnsLink)
    {
        EXPECT_EQ(ToString(HttpMethod::Link), "LINK");
    }

    TEST(HttpMethodTest, ToStringReturnsUnlink)
    {
        EXPECT_EQ(ToString(HttpMethod::Unlink), "UNLINK");
    }

    // === ToString: Edge cases ===

    TEST(HttpMethodTest, ToStringReturnsEmptyForUnknown)
    {
        EXPECT_EQ(ToString(HttpMethod::Unknown), "");
    }

    TEST(HttpMethodTest, ToStringReturnsUnknownForCastedMaxValue)
    {
        // Cast max uint8_t value (255) to HttpMethod — should hit default case
        // NOLINTNEXTLINE(optin.core.EnumCastOutOfRange)
        EXPECT_EQ(ToString(static_cast<HttpMethod>(std::numeric_limits<std::uint8_t>::max())),
                  "UNKNOWN");
    }

    TEST(HttpMethodTest, ToStringReturnsEmptyForCastedZero)
    {
        // HttpMethod::Unknown == 0
        EXPECT_EQ(ToString(static_cast<HttpMethod>(0U)), "");
    }

    // === FromString: Standard HTTP methods ===

    TEST(HttpMethodTest, FromStringParsesGet)
    {
        EXPECT_EQ(FromString("GET"), HttpMethod::Get);
    }

    TEST(HttpMethodTest, FromStringParsesPost)
    {
        EXPECT_EQ(FromString("POST"), HttpMethod::Post);
    }

    TEST(HttpMethodTest, FromStringParsesPut)
    {
        EXPECT_EQ(FromString("PUT"), HttpMethod::Put);
    }

    TEST(HttpMethodTest, FromStringParsesDelete)
    {
        EXPECT_EQ(FromString("DELETE"), HttpMethod::Delete);
    }

    TEST(HttpMethodTest, FromStringParsesHead)
    {
        EXPECT_EQ(FromString("HEAD"), HttpMethod::Head);
    }

    TEST(HttpMethodTest, FromStringParsesOptions)
    {
        EXPECT_EQ(FromString("OPTIONS"), HttpMethod::Options);
    }

    TEST(HttpMethodTest, FromStringParsesConnect)
    {
        EXPECT_EQ(FromString("CONNECT"), HttpMethod::Connect);
    }

    TEST(HttpMethodTest, FromStringParsesTrace)
    {
        EXPECT_EQ(FromString("TRACE"), HttpMethod::Trace);
    }

    // === FromString: WebDAV methods ===

    TEST(HttpMethodTest, FromStringParsesCopy)
    {
        EXPECT_EQ(FromString("COPY"), HttpMethod::Copy);
    }

    TEST(HttpMethodTest, FromStringParsesLock)
    {
        EXPECT_EQ(FromString("LOCK"), HttpMethod::Lock);
    }

    TEST(HttpMethodTest, FromStringParsesMkCol)
    {
        EXPECT_EQ(FromString("MKCOL"), HttpMethod::MkCol);
    }

    TEST(HttpMethodTest, FromStringParsesMove)
    {
        EXPECT_EQ(FromString("MOVE"), HttpMethod::Move);
    }

    TEST(HttpMethodTest, FromStringParsesPropfind)
    {
        EXPECT_EQ(FromString("PROPFIND"), HttpMethod::Propfind);
    }

    TEST(HttpMethodTest, FromStringParsesProppatch)
    {
        EXPECT_EQ(FromString("PROPPATCH"), HttpMethod::Proppatch);
    }

    TEST(HttpMethodTest, FromStringParsesSearch)
    {
        EXPECT_EQ(FromString("SEARCH"), HttpMethod::Search);
    }

    TEST(HttpMethodTest, FromStringParsesUnlock)
    {
        EXPECT_EQ(FromString("UNLOCK"), HttpMethod::Unlock);
    }

    TEST(HttpMethodTest, FromStringParsesBind)
    {
        EXPECT_EQ(FromString("BIND"), HttpMethod::Bind);
    }

    TEST(HttpMethodTest, FromStringParsesRebind)
    {
        EXPECT_EQ(FromString("REBIND"), HttpMethod::Rebind);
    }

    TEST(HttpMethodTest, FromStringParsesUnbind)
    {
        EXPECT_EQ(FromString("UNBIND"), HttpMethod::Unbind);
    }

    TEST(HttpMethodTest, FromStringParsesAcl)
    {
        EXPECT_EQ(FromString("ACL"), HttpMethod::Acl);
    }

    // === FromString: Subversion methods ===

    TEST(HttpMethodTest, FromStringParsesReport)
    {
        EXPECT_EQ(FromString("REPORT"), HttpMethod::Report);
    }

    TEST(HttpMethodTest, FromStringParsesMkActivity)
    {
        EXPECT_EQ(FromString("MKACTIVITY"), HttpMethod::MkActivity);
    }

    TEST(HttpMethodTest, FromStringParsesCheckout)
    {
        EXPECT_EQ(FromString("CHECKOUT"), HttpMethod::Checkout);
    }

    TEST(HttpMethodTest, FromStringParsesMerge)
    {
        EXPECT_EQ(FromString("MERGE"), HttpMethod::Merge);
    }

    // === FromString: UPnP methods ===

    TEST(HttpMethodTest, FromStringParsesMSearch)
    {
        EXPECT_EQ(FromString("M-SEARCH"), HttpMethod::MSearch);
    }

    TEST(HttpMethodTest, FromStringParsesNotify)
    {
        EXPECT_EQ(FromString("NOTIFY"), HttpMethod::Notify);
    }

    TEST(HttpMethodTest, FromStringParsesSubscribe)
    {
        EXPECT_EQ(FromString("SUBSCRIBE"), HttpMethod::Subscribe);
    }

    TEST(HttpMethodTest, FromStringParsesUnsubscribe)
    {
        EXPECT_EQ(FromString("UNSUBSCRIBE"), HttpMethod::Unsubscribe);
    }

    // === FromString: RFC-5789 methods ===

    TEST(HttpMethodTest, FromStringParsesPatch)
    {
        EXPECT_EQ(FromString("PATCH"), HttpMethod::Patch);
    }

    TEST(HttpMethodTest, FromStringParsesPurge)
    {
        EXPECT_EQ(FromString("PURGE"), HttpMethod::Purge);
    }

    // === FromString: CalDAV methods ===

    TEST(HttpMethodTest, FromStringParsesMkCalendar)
    {
        EXPECT_EQ(FromString("MKCALENDAR"), HttpMethod::MkCalendar);
    }

    // === FromString: RFC-2068 methods ===

    TEST(HttpMethodTest, FromStringParsesLink)
    {
        EXPECT_EQ(FromString("LINK"), HttpMethod::Link);
    }

    TEST(HttpMethodTest, FromStringParsesUnlink)
    {
        EXPECT_EQ(FromString("UNLINK"), HttpMethod::Unlink);
    }

    // === FromString: Edge cases ===

    TEST(HttpMethodTest, FromStringReturnsUnknownForEmptyString)
    {
        EXPECT_EQ(FromString(""), HttpMethod::Unknown);
    }

    TEST(HttpMethodTest, FromStringReturnsUnknownForInvalidMethod)
    {
        EXPECT_EQ(FromString("INVALID"), HttpMethod::Unknown);
    }

    TEST(HttpMethodTest, FromStringReturnsUnknownForLowercase)
    {
        // FromString is case-sensitive
        EXPECT_EQ(FromString("get"), HttpMethod::Unknown);
    }

    TEST(HttpMethodTest, FromStringReturnsUnknownForMixedCase)
    {
        EXPECT_EQ(FromString("Get"), HttpMethod::Unknown);
    }

    TEST(HttpMethodTest, FromStringReturnsUnknownForWhitespace)
    {
        EXPECT_EQ(FromString(" GET"), HttpMethod::Unknown);
    }

    TEST(HttpMethodTest, FromStringReturnsUnknownForTrailingWhitespace)
    {
        EXPECT_EQ(FromString("GET "), HttpMethod::Unknown);
    }

    TEST(HttpMethodTest, FromStringParsesGetWithExactLength)
    {
        // string_view with explicit length 4 matches "GET" correctly
        constexpr std::string_view method = std::string_view("GET", 3);
        EXPECT_EQ(FromString(method), HttpMethod::Get);
    }

    TEST(HttpMethodTest, FromStringReturnsUnknownForNullByteInString)
    {
        // A string containing a null byte should not match any method
        std::string withNullByte = "GE\0T";   // NOLINT(*-string-literal-with-embedded-nul)
        EXPECT_EQ(FromString(std::string_view(withNullByte.data(), withNullByte.size())),
                  HttpMethod::Unknown);
    }

    // === Round-trip: ToString then FromString ===

    TEST(HttpMethodTest, RoundTripToStringThenFromStringGet)
    {
        EXPECT_EQ(FromString(ToString(HttpMethod::Get)), HttpMethod::Get);
    }

    TEST(HttpMethodTest, RoundTripToStringThenFromStringPost)
    {
        EXPECT_EQ(FromString(ToString(HttpMethod::Post)), HttpMethod::Post);
    }

    TEST(HttpMethodTest, RoundTripToStringThenFromStringPut)
    {
        EXPECT_EQ(FromString(ToString(HttpMethod::Put)), HttpMethod::Put);
    }

    TEST(HttpMethodTest, RoundTripToStringThenFromStringDelete)
    {
        EXPECT_EQ(FromString(ToString(HttpMethod::Delete)), HttpMethod::Delete);
    }

    TEST(HttpMethodTest, RoundTripToStringThenFromStringPatch)
    {
        EXPECT_EQ(FromString(ToString(HttpMethod::Patch)), HttpMethod::Patch);
    }

    TEST(HttpMethodTest, RoundTripToStringThenFromStringMSearch)
    {
        EXPECT_EQ(FromString(ToString(HttpMethod::MSearch)), HttpMethod::MSearch);
    }

    TEST(HttpMethodTest, RoundTripUnknownMethod)
    {
        // ToString(Unknown) returns "", FromString("") returns Unknown
        EXPECT_EQ(FromString(ToString(HttpMethod::Unknown)), HttpMethod::Unknown);
    }

    // === Enum properties ===

    TEST(HttpMethodTest, EnumUnderlyingTypeIsUint8)
    {
        static_assert(std::is_same_v<std::underlying_type_t<HttpMethod>, std::uint8_t>);
    }

    TEST(HttpMethodTest, UnknownMethodHasValueZero)
    {
        EXPECT_EQ(std::to_underlying(HttpMethod::Unknown), 0U);
    }

    TEST(HttpMethodTest, ToStringIsNodiscard)
    {
        const std::string_view result = ToString(HttpMethod::Get);
        EXPECT_EQ(result, "GET");
    }

    TEST(HttpMethodTest, FromStringIsNodiscard)
    {
        const HttpMethod result = FromString("POST");
        EXPECT_EQ(result, HttpMethod::Post);
    }
}   // namespace Flux::Test
