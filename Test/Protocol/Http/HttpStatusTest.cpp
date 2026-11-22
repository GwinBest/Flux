#include "Protocol/Http/HttpStatus.hpp"

#include "gtest/gtest.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace Flux::Test
{
    // === ToInteger: 1xx Informational ===

    TEST(HttpStatusTest, ToIntegerReturns100ForContinue)
    {
        EXPECT_EQ(ToInteger(HttpStatus::Continue), 100U);
    }

    TEST(HttpStatusTest, ToIntegerReturns101ForSwitchingProtocols)
    {
        EXPECT_EQ(ToInteger(HttpStatus::SwitchingProtocols), 101U);
    }

    TEST(HttpStatusTest, ToIntegerReturns102ForProcessing)
    {
        EXPECT_EQ(ToInteger(HttpStatus::Processing), 102U);
    }

    TEST(HttpStatusTest, ToIntegerReturns103ForEarlyHints)
    {
        EXPECT_EQ(ToInteger(HttpStatus::EarlyHints), 103U);
    }

    // === ToInteger: 2xx Success ===

    TEST(HttpStatusTest, ToIntegerReturns200ForOk)
    {
        EXPECT_EQ(ToInteger(HttpStatus::Ok), 200U);
    }

    TEST(HttpStatusTest, ToIntegerReturns201ForCreated)
    {
        EXPECT_EQ(ToInteger(HttpStatus::Created), 201U);
    }

    TEST(HttpStatusTest, ToIntegerReturns202ForAccepted)
    {
        EXPECT_EQ(ToInteger(HttpStatus::Accepted), 202U);
    }

    TEST(HttpStatusTest, ToIntegerReturns203ForNonAuthoritativeInformation)
    {
        EXPECT_EQ(ToInteger(HttpStatus::NonAuthoritativeInformation), 203U);
    }

    TEST(HttpStatusTest, ToIntegerReturns204ForNoContent)
    {
        EXPECT_EQ(ToInteger(HttpStatus::NoContent), 204U);
    }

    TEST(HttpStatusTest, ToIntegerReturns205ForResetContent)
    {
        EXPECT_EQ(ToInteger(HttpStatus::ResetContent), 205U);
    }

    TEST(HttpStatusTest, ToIntegerReturns206ForPartialContent)
    {
        EXPECT_EQ(ToInteger(HttpStatus::PartialContent), 206U);
    }

    TEST(HttpStatusTest, ToIntegerReturns207ForMultiStatus)
    {
        EXPECT_EQ(ToInteger(HttpStatus::MultiStatus), 207U);
    }

    TEST(HttpStatusTest, ToIntegerReturns208ForAlreadyReported)
    {
        EXPECT_EQ(ToInteger(HttpStatus::AlreadyReported), 208U);
    }

    TEST(HttpStatusTest, ToIntegerReturns226ForImUsed)
    {
        EXPECT_EQ(ToInteger(HttpStatus::ImUsed), 226U);
    }

    // === ToInteger: 3xx Redirection ===

    TEST(HttpStatusTest, ToIntegerReturns300ForMultipleChoices)
    {
        EXPECT_EQ(ToInteger(HttpStatus::MultipleChoices), 300U);
    }

    TEST(HttpStatusTest, ToIntegerReturns301ForMovedPermanently)
    {
        EXPECT_EQ(ToInteger(HttpStatus::MovedPermanently), 301U);
    }

    TEST(HttpStatusTest, ToIntegerReturns302ForFound)
    {
        EXPECT_EQ(ToInteger(HttpStatus::Found), 302U);
    }

    TEST(HttpStatusTest, ToIntegerReturns303ForSeeOther)
    {
        EXPECT_EQ(ToInteger(HttpStatus::SeeOther), 303U);
    }

    TEST(HttpStatusTest, ToIntegerReturns304ForNotModified)
    {
        EXPECT_EQ(ToInteger(HttpStatus::NotModified), 304U);
    }

    TEST(HttpStatusTest, ToIntegerReturns305ForUseProxy)
    {
        EXPECT_EQ(ToInteger(HttpStatus::UseProxy), 305U);
    }

    TEST(HttpStatusTest, ToIntegerReturns307ForTemporaryRedirect)
    {
        EXPECT_EQ(ToInteger(HttpStatus::TemporaryRedirect), 307U);
    }

    TEST(HttpStatusTest, ToIntegerReturns308ForPermanentRedirect)
    {
        EXPECT_EQ(ToInteger(HttpStatus::PermanentRedirect), 308U);
    }

    // === ToInteger: 4xx Client Error ===

    TEST(HttpStatusTest, ToIntegerReturns400ForBadRequest)
    {
        EXPECT_EQ(ToInteger(HttpStatus::BadRequest), 400U);
    }

    TEST(HttpStatusTest, ToIntegerReturns401ForUnauthorized)
    {
        EXPECT_EQ(ToInteger(HttpStatus::Unauthorized), 401U);
    }

    TEST(HttpStatusTest, ToIntegerReturns402ForPaymentRequired)
    {
        EXPECT_EQ(ToInteger(HttpStatus::PaymentRequired), 402U);
    }

    TEST(HttpStatusTest, ToIntegerReturns403ForForbidden)
    {
        EXPECT_EQ(ToInteger(HttpStatus::Forbidden), 403U);
    }

    TEST(HttpStatusTest, ToIntegerReturns404ForNotFound)
    {
        EXPECT_EQ(ToInteger(HttpStatus::NotFound), 404U);
    }

    TEST(HttpStatusTest, ToIntegerReturns405ForMethodNotAllowed)
    {
        EXPECT_EQ(ToInteger(HttpStatus::MethodNotAllowed), 405U);
    }

    TEST(HttpStatusTest, ToIntegerReturns406ForNotAcceptable)
    {
        EXPECT_EQ(ToInteger(HttpStatus::NotAcceptable), 406U);
    }

    TEST(HttpStatusTest, ToIntegerReturns407ForProxyAuthenticationRequired)
    {
        EXPECT_EQ(ToInteger(HttpStatus::ProxyAuthenticationRequired), 407U);
    }

    TEST(HttpStatusTest, ToIntegerReturns408ForRequestTimeout)
    {
        EXPECT_EQ(ToInteger(HttpStatus::RequestTimeout), 408U);
    }

    TEST(HttpStatusTest, ToIntegerReturns409ForConflict)
    {
        EXPECT_EQ(ToInteger(HttpStatus::Conflict), 409U);
    }

    TEST(HttpStatusTest, ToIntegerReturns410ForGone)
    {
        EXPECT_EQ(ToInteger(HttpStatus::Gone), 410U);
    }

    TEST(HttpStatusTest, ToIntegerReturns411ForLengthRequired)
    {
        EXPECT_EQ(ToInteger(HttpStatus::LengthRequired), 411U);
    }

    TEST(HttpStatusTest, ToIntegerReturns412ForPreconditionFailed)
    {
        EXPECT_EQ(ToInteger(HttpStatus::PreconditionFailed), 412U);
    }

    TEST(HttpStatusTest, ToIntegerReturns413ForPayloadTooLarge)
    {
        EXPECT_EQ(ToInteger(HttpStatus::PayloadTooLarge), 413U);
    }

    TEST(HttpStatusTest, ToIntegerReturns414ForUriTooLong)
    {
        EXPECT_EQ(ToInteger(HttpStatus::UriTooLong), 414U);
    }

    TEST(HttpStatusTest, ToIntegerReturns415ForUnsupportedMediaType)
    {
        EXPECT_EQ(ToInteger(HttpStatus::UnsupportedMediaType), 415U);
    }

    TEST(HttpStatusTest, ToIntegerReturns416ForRangeNotSatisfiable)
    {
        EXPECT_EQ(ToInteger(HttpStatus::RangeNotSatisfiable), 416U);
    }

    TEST(HttpStatusTest, ToIntegerReturns417ForExpectationFailed)
    {
        EXPECT_EQ(ToInteger(HttpStatus::ExpectationFailed), 417U);
    }

    TEST(HttpStatusTest, ToIntegerReturns418ForImATeapot)
    {
        EXPECT_EQ(ToInteger(HttpStatus::ImATeapot), 418U);
    }

    TEST(HttpStatusTest, ToIntegerReturns421ForMisdirectedRequest)
    {
        EXPECT_EQ(ToInteger(HttpStatus::MisdirectedRequest), 421U);
    }

    TEST(HttpStatusTest, ToIntegerReturns422ForUnprocessableEntity)
    {
        EXPECT_EQ(ToInteger(HttpStatus::UnprocessableEntity), 422U);
    }

    TEST(HttpStatusTest, ToIntegerReturns423ForLocked)
    {
        EXPECT_EQ(ToInteger(HttpStatus::Locked), 423U);
    }

    TEST(HttpStatusTest, ToIntegerReturns424ForFailedDependency)
    {
        EXPECT_EQ(ToInteger(HttpStatus::FailedDependency), 424U);
    }

    TEST(HttpStatusTest, ToIntegerReturns425ForTooEarly)
    {
        EXPECT_EQ(ToInteger(HttpStatus::TooEarly), 425U);
    }

    TEST(HttpStatusTest, ToIntegerReturns426ForUpgradeRequired)
    {
        EXPECT_EQ(ToInteger(HttpStatus::UpgradeRequired), 426U);
    }

    TEST(HttpStatusTest, ToIntegerReturns428ForPreconditionRequired)
    {
        EXPECT_EQ(ToInteger(HttpStatus::PreconditionRequired), 428U);
    }

    TEST(HttpStatusTest, ToIntegerReturns429ForTooManyRequests)
    {
        EXPECT_EQ(ToInteger(HttpStatus::TooManyRequests), 429U);
    }

    TEST(HttpStatusTest, ToIntegerReturns431ForRequestHeaderFieldsTooLarge)
    {
        EXPECT_EQ(ToInteger(HttpStatus::RequestHeaderFieldsTooLarge), 431U);
    }

    TEST(HttpStatusTest, ToIntegerReturns451ForUnavailableForLegalReasons)
    {
        EXPECT_EQ(ToInteger(HttpStatus::UnavailableForLegalReasons), 451U);
    }

    // === ToInteger: 5xx Server Error ===

    TEST(HttpStatusTest, ToIntegerReturns500ForInternalServerError)
    {
        EXPECT_EQ(ToInteger(HttpStatus::InternalServerError), 500U);
    }

    TEST(HttpStatusTest, ToIntegerReturns501ForNotImplemented)
    {
        EXPECT_EQ(ToInteger(HttpStatus::NotImplemented), 501U);
    }

    TEST(HttpStatusTest, ToIntegerReturns502ForBadGateway)
    {
        EXPECT_EQ(ToInteger(HttpStatus::BadGateway), 502U);
    }

    TEST(HttpStatusTest, ToIntegerReturns503ForServiceUnavailable)
    {
        EXPECT_EQ(ToInteger(HttpStatus::ServiceUnavailable), 503U);
    }

    TEST(HttpStatusTest, ToIntegerReturns504ForGatewayTimeout)
    {
        EXPECT_EQ(ToInteger(HttpStatus::GatewayTimeout), 504U);
    }

    TEST(HttpStatusTest, ToIntegerReturns505ForHttpVersionNotSupported)
    {
        EXPECT_EQ(ToInteger(HttpStatus::HttpVersionNotSupported), 505U);
    }

    TEST(HttpStatusTest, ToIntegerReturns506ForVariantAlsoNegotiates)
    {
        EXPECT_EQ(ToInteger(HttpStatus::VariantAlsoNegotiates), 506U);
    }

    TEST(HttpStatusTest, ToIntegerReturns507ForInsufficientStorage)
    {
        EXPECT_EQ(ToInteger(HttpStatus::InsufficientStorage), 507U);
    }

    TEST(HttpStatusTest, ToIntegerReturns508ForLoopDetected)
    {
        EXPECT_EQ(ToInteger(HttpStatus::LoopDetected), 508U);
    }

    TEST(HttpStatusTest, ToIntegerReturns510ForNotExtended)
    {
        EXPECT_EQ(ToInteger(HttpStatus::NotExtended), 510U);
    }

    TEST(HttpStatusTest, ToIntegerReturns511ForNetworkAuthenticationRequired)
    {
        EXPECT_EQ(ToInteger(HttpStatus::NetworkAuthenticationRequired), 511U);
    }

    // === Edge cases ===

    TEST(HttpStatusTest, EnumUnderlyingTypeIsUint16)
    {
        static_assert(std::is_same_v<std::underlying_type_t<HttpStatus>, std::uint16_t>);
    }

    TEST(HttpStatusTest, ToIntegerIsNodiscard)
    {
        const std::uint16_t result = ToInteger(HttpStatus::Ok);
        EXPECT_EQ(result, 200U);
    }

    TEST(HttpStatusTest, ToIntegerForCastedMinValue)
    {
        // Cast 0 to HttpStatus — should return 0
        EXPECT_EQ(ToInteger(static_cast<HttpStatus>(0U)), 0U);
    }

    TEST(HttpStatusTest, ToIntegerForCastedMaxValue)
    {
        // Cast max uint16_t value to HttpStatus — should return max value
        EXPECT_EQ(ToInteger(static_cast<HttpStatus>(std::numeric_limits<std::uint16_t>::max())),
                  std::numeric_limits<std::uint16_t>::max());
    }

    TEST(HttpStatusTest, ToIntegerForCastedOne)
    {
        // Cast 1 to HttpStatus — should return 1
        EXPECT_EQ(ToInteger(static_cast<HttpStatus>(1U)), 1U);
    }

    // === Status code ranges ===

    TEST(HttpStatusTest, All1xxStatusesAreInRange)
    {
        EXPECT_GE(ToInteger(HttpStatus::Continue), 100U);
        EXPECT_LT(ToInteger(HttpStatus::Continue), 200U);
        EXPECT_GE(ToInteger(HttpStatus::SwitchingProtocols), 100U);
        EXPECT_LT(ToInteger(HttpStatus::SwitchingProtocols), 200U);
        EXPECT_GE(ToInteger(HttpStatus::Processing), 100U);
        EXPECT_LT(ToInteger(HttpStatus::Processing), 200U);
        EXPECT_GE(ToInteger(HttpStatus::EarlyHints), 100U);
        EXPECT_LT(ToInteger(HttpStatus::EarlyHints), 200U);
    }

    TEST(HttpStatusTest, All2xxStatusesAreInRange)
    {
        EXPECT_GE(ToInteger(HttpStatus::Ok), 200U);
        EXPECT_LT(ToInteger(HttpStatus::Ok), 300U);
        EXPECT_GE(ToInteger(HttpStatus::Created), 200U);
        EXPECT_LT(ToInteger(HttpStatus::Created), 300U);
        EXPECT_GE(ToInteger(HttpStatus::NoContent), 200U);
        EXPECT_LT(ToInteger(HttpStatus::NoContent), 300U);
        EXPECT_GE(ToInteger(HttpStatus::ImUsed), 200U);
        EXPECT_LT(ToInteger(HttpStatus::ImUsed), 300U);
    }

    TEST(HttpStatusTest, All3xxStatusesAreInRange)
    {
        EXPECT_GE(ToInteger(HttpStatus::MultipleChoices), 300U);
        EXPECT_LT(ToInteger(HttpStatus::MultipleChoices), 400U);
        EXPECT_GE(ToInteger(HttpStatus::Found), 300U);
        EXPECT_LT(ToInteger(HttpStatus::Found), 400U);
        EXPECT_GE(ToInteger(HttpStatus::PermanentRedirect), 300U);
        EXPECT_LT(ToInteger(HttpStatus::PermanentRedirect), 400U);
    }

    TEST(HttpStatusTest, All4xxStatusesAreInRange)
    {
        EXPECT_GE(ToInteger(HttpStatus::BadRequest), 400U);
        EXPECT_LT(ToInteger(HttpStatus::BadRequest), 500U);
        EXPECT_GE(ToInteger(HttpStatus::NotFound), 400U);
        EXPECT_LT(ToInteger(HttpStatus::NotFound), 500U);
        EXPECT_GE(ToInteger(HttpStatus::UnavailableForLegalReasons), 400U);
        EXPECT_LT(ToInteger(HttpStatus::UnavailableForLegalReasons), 500U);
    }

    TEST(HttpStatusTest, All5xxStatusesAreInRange)
    {
        EXPECT_GE(ToInteger(HttpStatus::InternalServerError), 500U);
        EXPECT_LT(ToInteger(HttpStatus::InternalServerError), 600U);
        EXPECT_GE(ToInteger(HttpStatus::BadGateway), 500U);
        EXPECT_LT(ToInteger(HttpStatus::BadGateway), 600U);
        EXPECT_GE(ToInteger(HttpStatus::NetworkAuthenticationRequired), 500U);
        EXPECT_LT(ToInteger(HttpStatus::NetworkAuthenticationRequired), 600U);
    }

    // === Uniqueness: all status codes are distinct ===

    TEST(HttpStatusTest, AllStatusCodesAreUnique)
    {
        // Collect all status codes and verify uniqueness
        const std::array codes {
            ToInteger(HttpStatus::Continue),
            ToInteger(HttpStatus::SwitchingProtocols),
            ToInteger(HttpStatus::Processing),
            ToInteger(HttpStatus::EarlyHints),
            ToInteger(HttpStatus::Ok),
            ToInteger(HttpStatus::Created),
            ToInteger(HttpStatus::Accepted),
            ToInteger(HttpStatus::NonAuthoritativeInformation),
            ToInteger(HttpStatus::NoContent),
            ToInteger(HttpStatus::ResetContent),
            ToInteger(HttpStatus::PartialContent),
            ToInteger(HttpStatus::MultiStatus),
            ToInteger(HttpStatus::AlreadyReported),
            ToInteger(HttpStatus::ImUsed),
            ToInteger(HttpStatus::MultipleChoices),
            ToInteger(HttpStatus::MovedPermanently),
            ToInteger(HttpStatus::Found),
            ToInteger(HttpStatus::SeeOther),
            ToInteger(HttpStatus::NotModified),
            ToInteger(HttpStatus::UseProxy),
            ToInteger(HttpStatus::TemporaryRedirect),
            ToInteger(HttpStatus::PermanentRedirect),
            ToInteger(HttpStatus::BadRequest),
            ToInteger(HttpStatus::Unauthorized),
            ToInteger(HttpStatus::PaymentRequired),
            ToInteger(HttpStatus::Forbidden),
            ToInteger(HttpStatus::NotFound),
            ToInteger(HttpStatus::MethodNotAllowed),
            ToInteger(HttpStatus::NotAcceptable),
            ToInteger(HttpStatus::ProxyAuthenticationRequired),
            ToInteger(HttpStatus::RequestTimeout),
            ToInteger(HttpStatus::Conflict),
            ToInteger(HttpStatus::Gone),
            ToInteger(HttpStatus::LengthRequired),
            ToInteger(HttpStatus::PreconditionFailed),
            ToInteger(HttpStatus::PayloadTooLarge),
            ToInteger(HttpStatus::UriTooLong),
            ToInteger(HttpStatus::UnsupportedMediaType),
            ToInteger(HttpStatus::RangeNotSatisfiable),
            ToInteger(HttpStatus::ExpectationFailed),
            ToInteger(HttpStatus::ImATeapot),
            ToInteger(HttpStatus::MisdirectedRequest),
            ToInteger(HttpStatus::UnprocessableEntity),
            ToInteger(HttpStatus::Locked),
            ToInteger(HttpStatus::FailedDependency),
            ToInteger(HttpStatus::TooEarly),
            ToInteger(HttpStatus::UpgradeRequired),
            ToInteger(HttpStatus::PreconditionRequired),
            ToInteger(HttpStatus::TooManyRequests),
            ToInteger(HttpStatus::RequestHeaderFieldsTooLarge),
            ToInteger(HttpStatus::UnavailableForLegalReasons),
            ToInteger(HttpStatus::InternalServerError),
            ToInteger(HttpStatus::NotImplemented),
            ToInteger(HttpStatus::BadGateway),
            ToInteger(HttpStatus::ServiceUnavailable),
            ToInteger(HttpStatus::GatewayTimeout),
            ToInteger(HttpStatus::HttpVersionNotSupported),
            ToInteger(HttpStatus::VariantAlsoNegotiates),
            ToInteger(HttpStatus::InsufficientStorage),
            ToInteger(HttpStatus::LoopDetected),
            ToInteger(HttpStatus::NotExtended),
            ToInteger(HttpStatus::NetworkAuthenticationRequired),
        };

        // Simple uniqueness check: sort and compare adjacent
        constexpr auto count = std::size(codes);
        for (std::size_t i = 0; i < count; ++i)
        {
            for (std::size_t j = i + 1; j < count; ++j)
            {
                EXPECT_NE(codes[i],   // NOLINT(*-pro-bounds-avoid-unchecked-container-access,
                                      // *-pro-bounds-constant-array-index)
                          codes[j])   // NOLINT(*-pro-bounds-avoid-unchecked-container-access,
                                      // *-pro-bounds-constant-array-index)
                    << "Duplicate status code at indices " << i << " and " << j;
            }
        }
    }
}   // namespace Flux::Test
