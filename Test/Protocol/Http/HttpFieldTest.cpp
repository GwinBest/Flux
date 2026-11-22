#include "Protocol/Http/HttpField.hpp"

#include "gtest/gtest.h"

#include <cstdint>
#include <string_view>

namespace Flux::Test
{
    // === ToString: Standard request fields ===

    TEST(HttpFieldTest, ToStringReturnsAccept)
    {
        EXPECT_EQ(ToString(HttpField::Accept), "Accept");
    }

    TEST(HttpFieldTest, ToStringReturnsAcceptCH)
    {
        EXPECT_EQ(ToString(HttpField::AcceptCH), "Accept-CH");
    }

    TEST(HttpFieldTest, ToStringReturnsAcceptCharset)
    {
        EXPECT_EQ(ToString(HttpField::AcceptCharset), "Accept-Charset");
    }

    TEST(HttpFieldTest, ToStringReturnsAcceptEncoding)
    {
        EXPECT_EQ(ToString(HttpField::AcceptEncoding), "Accept-Encoding");
    }

    TEST(HttpFieldTest, ToStringReturnsAcceptLanguage)
    {
        EXPECT_EQ(ToString(HttpField::AcceptLanguage), "Accept-Language");
    }

    TEST(HttpFieldTest, ToStringReturnsAcceptPatch)
    {
        EXPECT_EQ(ToString(HttpField::AcceptPatch), "Accept-Patch");
    }

    TEST(HttpFieldTest, ToStringReturnsAcceptPost)
    {
        EXPECT_EQ(ToString(HttpField::AcceptPost), "Accept-Post");
    }

    TEST(HttpFieldTest, ToStringReturnsAcceptRanges)
    {
        EXPECT_EQ(ToString(HttpField::AcceptRanges), "Accept-Ranges");
    }

    TEST(HttpFieldTest, ToStringReturnsAcceptSignature)
    {
        EXPECT_EQ(ToString(HttpField::AcceptSignature), "Accept-Signature");
    }

    // === ToString: CORS fields ===

    TEST(HttpFieldTest, ToStringReturnsAccessControlAllowCredentials)
    {
        EXPECT_EQ(ToString(HttpField::AccessControlAllowCredentials),
                  "Access-Control-Allow-Credentials");
    }

    TEST(HttpFieldTest, ToStringReturnsAccessControlAllowHeaders)
    {
        EXPECT_EQ(ToString(HttpField::AccessControlAllowHeaders), "Access-Control-Allow-Headers");
    }

    TEST(HttpFieldTest, ToStringReturnsAccessControlAllowMethods)
    {
        EXPECT_EQ(ToString(HttpField::AccessControlAllowMethods), "Access-Control-Allow-Methods");
    }

    TEST(HttpFieldTest, ToStringReturnsAccessControlAllowOrigin)
    {
        EXPECT_EQ(ToString(HttpField::AccessControlAllowOrigin), "Access-Control-Allow-Origin");
    }

    TEST(HttpFieldTest, ToStringReturnsAccessControlExposeHeaders)
    {
        EXPECT_EQ(ToString(HttpField::AccessControlExposeHeaders), "Access-Control-Expose-Headers");
    }

    TEST(HttpFieldTest, ToStringReturnsAccessControlMaxAge)
    {
        EXPECT_EQ(ToString(HttpField::AccessControlMaxAge), "Access-Control-Max-Age");
    }

    TEST(HttpFieldTest, ToStringReturnsAccessControlRequestHeaders)
    {
        EXPECT_EQ(ToString(HttpField::AccessControlRequestHeaders),
                  "Access-Control-Request-Headers");
    }

    TEST(HttpFieldTest, ToStringReturnsAccessControlRequestMethod)
    {
        EXPECT_EQ(ToString(HttpField::AccessControlRequestMethod), "Access-Control-Request-Method");
    }

    // === ToString: Common fields ===

    TEST(HttpFieldTest, ToStringReturnsAge)
    {
        EXPECT_EQ(ToString(HttpField::Age), "Age");
    }

    TEST(HttpFieldTest, ToStringReturnsAllow)
    {
        EXPECT_EQ(ToString(HttpField::Allow), "Allow");
    }

    TEST(HttpFieldTest, ToStringReturnsAltSvc)
    {
        EXPECT_EQ(ToString(HttpField::AltSvc), "Alt-Svc");
    }

    TEST(HttpFieldTest, ToStringReturnsAltUsed)
    {
        EXPECT_EQ(ToString(HttpField::AltUsed), "Alt-Used");
    }

    TEST(HttpFieldTest, ToStringReturnsAuthorization)
    {
        EXPECT_EQ(ToString(HttpField::Authorization), "Authorization");
    }

    TEST(HttpFieldTest, ToStringReturnsCacheControl)
    {
        EXPECT_EQ(ToString(HttpField::CacheControl), "Cache-Control");
    }

    TEST(HttpFieldTest, ToStringReturnsClearSiteData)
    {
        EXPECT_EQ(ToString(HttpField::ClearSiteData), "Clear-Site-Data");
    }

    TEST(HttpFieldTest, ToStringReturnsConnection)
    {
        EXPECT_EQ(ToString(HttpField::Connection), "Connection");
    }

    TEST(HttpFieldTest, ToStringReturnsContentDigest)
    {
        EXPECT_EQ(ToString(HttpField::ContentDigest), "Content-Digest");
    }

    TEST(HttpFieldTest, ToStringReturnsContentDisposition)
    {
        EXPECT_EQ(ToString(HttpField::ContentDisposition), "Content-Disposition");
    }

    TEST(HttpFieldTest, ToStringReturnsContentDPR)
    {
        EXPECT_EQ(ToString(HttpField::ContentDPR), "Content-DPR");
    }

    TEST(HttpFieldTest, ToStringReturnsContentEncoding)
    {
        EXPECT_EQ(ToString(HttpField::ContentEncoding), "Content-Encoding");
    }

    TEST(HttpFieldTest, ToStringReturnsContentLanguage)
    {
        EXPECT_EQ(ToString(HttpField::ContentLanguage), "Content-Language");
    }

    TEST(HttpFieldTest, ToStringReturnsContentLength)
    {
        EXPECT_EQ(ToString(HttpField::ContentLength), "Content-Length");
    }

    TEST(HttpFieldTest, ToStringReturnsContentLocation)
    {
        EXPECT_EQ(ToString(HttpField::ContentLocation), "Content-Location");
    }

    TEST(HttpFieldTest, ToStringReturnsContentRange)
    {
        EXPECT_EQ(ToString(HttpField::ContentRange), "Content-Range");
    }

    TEST(HttpFieldTest, ToStringReturnsContentSecurityPolicy)
    {
        EXPECT_EQ(ToString(HttpField::ContentSecurityPolicy), "Content-Security-Policy");
    }

    TEST(HttpFieldTest, ToStringReturnsContentSecurityPolicyReportOnly)
    {
        EXPECT_EQ(ToString(HttpField::ContentSecurityPolicyReportOnly),
                  "Content-Security-Policy-Report-Only");
    }

    TEST(HttpFieldTest, ToStringReturnsContentType)
    {
        EXPECT_EQ(ToString(HttpField::ContentType), "Content-Type");
    }

    TEST(HttpFieldTest, ToStringReturnsCookie)
    {
        EXPECT_EQ(ToString(HttpField::Cookie), "Cookie");
    }

    // === ToString: Cross-Origin Policy fields ===

    TEST(HttpFieldTest, ToStringReturnsCrossOriginEmbedderPolicy)
    {
        EXPECT_EQ(ToString(HttpField::CrossOriginEmbedderPolicy), "Cross-Origin-Embedder-Policy");
    }

    TEST(HttpFieldTest, ToStringReturnsCrossOriginOpenerPolicy)
    {
        EXPECT_EQ(ToString(HttpField::CrossOriginOpenerPolicy), "Cross-Origin-Opener-Policy");
    }

    TEST(HttpFieldTest, ToStringReturnsCrossOriginResourcePolicy)
    {
        EXPECT_EQ(ToString(HttpField::CrossOriginResourcePolicy), "Cross-Origin-Resource-Policy");
    }

    // === ToString: Date & Deprecation fields ===

    TEST(HttpFieldTest, ToStringReturnsDate)
    {
        EXPECT_EQ(ToString(HttpField::Date), "Date");
    }

    TEST(HttpFieldTest, ToStringReturnsDeprecation)
    {
        EXPECT_EQ(ToString(HttpField::Deprecation), "Deprecation");
    }

    TEST(HttpFieldTest, ToStringReturnsDeviceMemory)
    {
        EXPECT_EQ(ToString(HttpField::DeviceMemory), "Device-Memory");
    }

    TEST(HttpFieldTest, ToStringReturnsDigest)
    {
        EXPECT_EQ(ToString(HttpField::Digest), "Digest");
    }

    TEST(HttpFieldTest, ToStringReturnsDNT)
    {
        EXPECT_EQ(ToString(HttpField::DNT), "DNT");
    }

    TEST(HttpFieldTest, ToStringReturnsDPR)
    {
        EXPECT_EQ(ToString(HttpField::DPR), "DPR");
    }

    TEST(HttpFieldTest, ToStringReturnsETag)
    {
        EXPECT_EQ(ToString(HttpField::ETag), "ETag");
    }

    TEST(HttpFieldTest, ToStringReturnsExpect)
    {
        EXPECT_EQ(ToString(HttpField::Expect), "Expect");
    }

    TEST(HttpFieldTest, ToStringReturnsExpectCT)
    {
        EXPECT_EQ(ToString(HttpField::ExpectCT), "Expect-CT");
    }

    TEST(HttpFieldTest, ToStringReturnsExpires)
    {
        EXPECT_EQ(ToString(HttpField::Expires), "Expires");
    }

    TEST(HttpFieldTest, ToStringReturnsForwarded)
    {
        EXPECT_EQ(ToString(HttpField::Forwarded), "Forwarded");
    }

    TEST(HttpFieldTest, ToStringReturnsFrom)
    {
        EXPECT_EQ(ToString(HttpField::From), "From");
    }

    TEST(HttpFieldTest, ToStringReturnsHost)
    {
        EXPECT_EQ(ToString(HttpField::Host), "Host");
    }

    TEST(HttpFieldTest, ToStringReturnsHTTP2Settings)
    {
        EXPECT_EQ(ToString(HttpField::HTTP2Settings), "HTTP2-Settings");
    }

    // === ToString: Conditional fields ===

    TEST(HttpFieldTest, ToStringReturnsIfMatch)
    {
        EXPECT_EQ(ToString(HttpField::IfMatch), "If-Match");
    }

    TEST(HttpFieldTest, ToStringReturnsIfModifiedSince)
    {
        EXPECT_EQ(ToString(HttpField::IfModifiedSince), "If-Modified-Since");
    }

    TEST(HttpFieldTest, ToStringReturnsIfNoneMatch)
    {
        EXPECT_EQ(ToString(HttpField::IfNoneMatch), "If-None-Match");
    }

    TEST(HttpFieldTest, ToStringReturnsIfRange)
    {
        EXPECT_EQ(ToString(HttpField::IfRange), "If-Range");
    }

    TEST(HttpFieldTest, ToStringReturnsIfUnmodifiedSince)
    {
        EXPECT_EQ(ToString(HttpField::IfUnmodifiedSince), "If-Unmodified-Since");
    }

    // === ToString: More common fields ===

    TEST(HttpFieldTest, ToStringReturnsKeepAlive)
    {
        EXPECT_EQ(ToString(HttpField::KeepAlive), "Keep-Alive");
    }

    TEST(HttpFieldTest, ToStringReturnsLastModified)
    {
        EXPECT_EQ(ToString(HttpField::LastModified), "Last-Modified");
    }

    TEST(HttpFieldTest, ToStringReturnsLink)
    {
        EXPECT_EQ(ToString(HttpField::Link), "Link");
    }

    TEST(HttpFieldTest, ToStringReturnsLocation)
    {
        EXPECT_EQ(ToString(HttpField::Location), "Location");
    }

    TEST(HttpFieldTest, ToStringReturnsMaxForwards)
    {
        EXPECT_EQ(ToString(HttpField::MaxForwards), "Max-Forwards");
    }

    TEST(HttpFieldTest, ToStringReturnsOrigin)
    {
        EXPECT_EQ(ToString(HttpField::Origin), "Origin");
    }

    TEST(HttpFieldTest, ToStringReturnsOriginAgentCluster)
    {
        EXPECT_EQ(ToString(HttpField::OriginAgentCluster), "Origin-Agent-Cluster");
    }

    TEST(HttpFieldTest, ToStringReturnsPragma)
    {
        EXPECT_EQ(ToString(HttpField::Pragma), "Pragma");
    }

    TEST(HttpFieldTest, ToStringReturnsPrefer)
    {
        EXPECT_EQ(ToString(HttpField::Prefer), "Prefer");
    }

    TEST(HttpFieldTest, ToStringReturnsPreferenceApplied)
    {
        EXPECT_EQ(ToString(HttpField::PreferenceApplied), "Preference-Applied");
    }

    TEST(HttpFieldTest, ToStringReturnsPriority)
    {
        EXPECT_EQ(ToString(HttpField::Priority), "Priority");
    }

    TEST(HttpFieldTest, ToStringReturnsProxyAuthenticate)
    {
        EXPECT_EQ(ToString(HttpField::ProxyAuthenticate), "Proxy-Authenticate");
    }

    TEST(HttpFieldTest, ToStringReturnsProxyAuthorization)
    {
        EXPECT_EQ(ToString(HttpField::ProxyAuthorization), "Proxy-Authorization");
    }

    TEST(HttpFieldTest, ToStringReturnsProxyConnection)
    {
        EXPECT_EQ(ToString(HttpField::ProxyConnection), "Proxy-Connection");
    }

    TEST(HttpFieldTest, ToStringReturnsRange)
    {
        EXPECT_EQ(ToString(HttpField::Range), "Range");
    }

    TEST(HttpFieldTest, ToStringReturnsReferer)
    {
        EXPECT_EQ(ToString(HttpField::Referer), "Referer");
    }

    TEST(HttpFieldTest, ToStringReturnsReferrerPolicy)
    {
        EXPECT_EQ(ToString(HttpField::ReferrerPolicy), "Referrer-Policy");
    }

    TEST(HttpFieldTest, ToStringReturnsRefresh)
    {
        EXPECT_EQ(ToString(HttpField::Refresh), "Refresh");
    }

    TEST(HttpFieldTest, ToStringReturnsReportTo)
    {
        EXPECT_EQ(ToString(HttpField::ReportTo), "Report-To");
    }

    TEST(HttpFieldTest, ToStringReturnsReportingEndpoints)
    {
        EXPECT_EQ(ToString(HttpField::ReportingEndpoints), "Reporting-Endpoints");
    }

    TEST(HttpFieldTest, ToStringReturnsReprDigest)
    {
        EXPECT_EQ(ToString(HttpField::ReprDigest), "Repr-Digest");
    }

    TEST(HttpFieldTest, ToStringReturnsRetryAfter)
    {
        EXPECT_EQ(ToString(HttpField::RetryAfter), "Retry-After");
    }

    // === ToString: Security & Client Hints fields ===

    TEST(HttpFieldTest, ToStringReturnsSecCHUAFullVersion)
    {
        EXPECT_EQ(ToString(HttpField::SecCHUAFullVersion), "Sec-CH-UA-Full-Version");
    }

    TEST(HttpFieldTest, ToStringReturnsSecFetchDest)
    {
        EXPECT_EQ(ToString(HttpField::SecFetchDest), "Sec-Fetch-Dest");
    }

    TEST(HttpFieldTest, ToStringReturnsSecFetchMode)
    {
        EXPECT_EQ(ToString(HttpField::SecFetchMode), "Sec-Fetch-Mode");
    }

    TEST(HttpFieldTest, ToStringReturnsSecFetchSite)
    {
        EXPECT_EQ(ToString(HttpField::SecFetchSite), "Sec-Fetch-Site");
    }

    TEST(HttpFieldTest, ToStringReturnsSecFetchUser)
    {
        EXPECT_EQ(ToString(HttpField::SecFetchUser), "Sec-Fetch-User");
    }

    TEST(HttpFieldTest, ToStringReturnsSecPurpose)
    {
        EXPECT_EQ(ToString(HttpField::SecPurpose), "Sec-Purpose");
    }

    // === ToString: WebSocket fields ===

    TEST(HttpFieldTest, ToStringReturnsSecWebSocketAccept)
    {
        EXPECT_EQ(ToString(HttpField::SecWebSocketAccept), "Sec-WebSocket-Accept");
    }

    TEST(HttpFieldTest, ToStringReturnsSecWebSocketExtensions)
    {
        EXPECT_EQ(ToString(HttpField::SecWebSocketExtensions), "Sec-WebSocket-Extensions");
    }

    TEST(HttpFieldTest, ToStringReturnsSecWebSocketKey)
    {
        EXPECT_EQ(ToString(HttpField::SecWebSocketKey), "Sec-WebSocket-Key");
    }

    TEST(HttpFieldTest, ToStringReturnsSecWebSocketProtocol)
    {
        EXPECT_EQ(ToString(HttpField::SecWebSocketProtocol), "Sec-WebSocket-Protocol");
    }

    TEST(HttpFieldTest, ToStringReturnsSecWebSocketVersion)
    {
        EXPECT_EQ(ToString(HttpField::SecWebSocketVersion), "Sec-WebSocket-Version");
    }

    // === ToString: Server fields ===

    TEST(HttpFieldTest, ToStringReturnsServer)
    {
        EXPECT_EQ(ToString(HttpField::Server), "Server");
    }

    TEST(HttpFieldTest, ToStringReturnsServerTiming)
    {
        EXPECT_EQ(ToString(HttpField::ServerTiming), "Server-Timing");
    }

    // === ToString: Service Worker fields ===

    TEST(HttpFieldTest, ToStringReturnsServiceWorker)
    {
        EXPECT_EQ(ToString(HttpField::ServiceWorker), "Service-Worker");
    }

    TEST(HttpFieldTest, ToStringReturnsServiceWorkerAllowed)
    {
        EXPECT_EQ(ToString(HttpField::ServiceWorkerAllowed), "Service-Worker-Allowed");
    }

    TEST(HttpFieldTest, ToStringReturnsServiceWorkerNavigationPreload)
    {
        EXPECT_EQ(ToString(HttpField::ServiceWorkerNavigationPreload),
                  "Service-Worker-Navigation-Preload");
    }

    TEST(HttpFieldTest, ToStringReturnsSetCookie)
    {
        EXPECT_EQ(ToString(HttpField::SetCookie), "Set-Cookie");
    }

    TEST(HttpFieldTest, ToStringReturnsSetLogin)
    {
        EXPECT_EQ(ToString(HttpField::SetLogin), "Set-Login");
    }

    TEST(HttpFieldTest, ToStringReturnsSignature)
    {
        EXPECT_EQ(ToString(HttpField::Signature), "Signature");
    }

    TEST(HttpFieldTest, ToStringReturnsSignatureInput)
    {
        EXPECT_EQ(ToString(HttpField::SignatureInput), "Signature-Input");
    }

    TEST(HttpFieldTest, ToStringReturnsSourcemap)
    {
        EXPECT_EQ(ToString(HttpField::Sourcemap), "SourceMap");
    }

    TEST(HttpFieldTest, ToStringReturnsStrictTransportSecurity)
    {
        EXPECT_EQ(ToString(HttpField::StrictTransportSecurity), "Strict-Transport-Security");
    }

    TEST(HttpFieldTest, ToStringReturnsTE)
    {
        EXPECT_EQ(ToString(HttpField::TE), "TE");
    }

    TEST(HttpFieldTest, ToStringReturnsTimingAllowOrigin)
    {
        EXPECT_EQ(ToString(HttpField::TimingAllowOrigin), "Timing-Allow-Origin");
    }

    TEST(HttpFieldTest, ToStringReturnsTK)
    {
        EXPECT_EQ(ToString(HttpField::TK), "TK");
    }

    TEST(HttpFieldTest, ToStringReturnsTrailer)
    {
        EXPECT_EQ(ToString(HttpField::Trailer), "Trailer");
    }

    TEST(HttpFieldTest, ToStringReturnsTransferEncoding)
    {
        EXPECT_EQ(ToString(HttpField::TransferEncoding), "Transfer-Encoding");
    }

    TEST(HttpFieldTest, ToStringReturnsUpgrade)
    {
        EXPECT_EQ(ToString(HttpField::Upgrade), "Upgrade");
    }

    TEST(HttpFieldTest, ToStringReturnsUpgradeInsecureRequests)
    {
        EXPECT_EQ(ToString(HttpField::UpgradeInsecureRequests), "Upgrade-Insecure-Requests");
    }

    TEST(HttpFieldTest, ToStringReturnsUserAgent)
    {
        EXPECT_EQ(ToString(HttpField::UserAgent), "User-Agent");
    }

    TEST(HttpFieldTest, ToStringReturnsVary)
    {
        EXPECT_EQ(ToString(HttpField::Vary), "Vary");
    }

    TEST(HttpFieldTest, ToStringReturnsVia)
    {
        EXPECT_EQ(ToString(HttpField::Via), "Via");
    }

    TEST(HttpFieldTest, ToStringReturnsViewportWidth)
    {
        EXPECT_EQ(ToString(HttpField::ViewportWidth), "Viewport-Width");
    }

    TEST(HttpFieldTest, ToStringReturnsWantContentDigest)
    {
        EXPECT_EQ(ToString(HttpField::WantContentDigest), "Want-Content-Digest");
    }

    TEST(HttpFieldTest, ToStringReturnsWantReprDigest)
    {
        EXPECT_EQ(ToString(HttpField::WantReprDigest), "Want-Repr-Digest");
    }

    TEST(HttpFieldTest, ToStringReturnsWarning)
    {
        EXPECT_EQ(ToString(HttpField::Warning), "Warning");
    }

    TEST(HttpFieldTest, ToStringReturnsWidth)
    {
        EXPECT_EQ(ToString(HttpField::Width), "Width");
    }

    TEST(HttpFieldTest, ToStringReturnsWwwAuthenticate)
    {
        EXPECT_EQ(ToString(HttpField::WwwAuthenticate), "WWW-Authenticate");
    }

    // === ToString: X- prefixed fields ===

    TEST(HttpFieldTest, ToStringReturnsXContentTypeOptions)
    {
        EXPECT_EQ(ToString(HttpField::XContentTypeOptions), "X-Content-Type-Options");
    }

    TEST(HttpFieldTest, ToStringReturnsXDNSPrefetchControl)
    {
        EXPECT_EQ(ToString(HttpField::XDNSPrefetchControl), "X-DNS-Prefetch-Control");
    }

    TEST(HttpFieldTest, ToStringReturnsXForwardedFor)
    {
        EXPECT_EQ(ToString(HttpField::XForwardedFor), "X-Forwarded-For");
    }

    TEST(HttpFieldTest, ToStringReturnsXForwardedHost)
    {
        EXPECT_EQ(ToString(HttpField::XForwardedHost), "X-Forwarded-Host");
    }

    TEST(HttpFieldTest, ToStringReturnsXForwardedProto)
    {
        EXPECT_EQ(ToString(HttpField::XForwardedProto), "X-Forwarded-Proto");
    }

    TEST(HttpFieldTest, ToStringReturnsXFrameOptions)
    {
        EXPECT_EQ(ToString(HttpField::XFrameOptions), "X-Frame-Options");
    }

    TEST(HttpFieldTest, ToStringReturnsXPermittedCrossDomainPolicies)
    {
        EXPECT_EQ(ToString(HttpField::XPermittedCrossDomainPolicies),
                  "X-Permitted-Cross-Domain-Policies");
    }

    TEST(HttpFieldTest, ToStringReturnsXPoweredBy)
    {
        EXPECT_EQ(ToString(HttpField::XPoweredBy), "X-Powered-By");
    }

    TEST(HttpFieldTest, ToStringReturnsXRobotsTag)
    {
        EXPECT_EQ(ToString(HttpField::XRobotsTag), "X-Robots-Tag");
    }

    TEST(HttpFieldTest, ToStringReturnsXXSSProtection)
    {
        EXPECT_EQ(ToString(HttpField::XXSSProtection), "X-XSS-Protection");
    }

    // === Edge cases ===

    TEST(HttpFieldTest, ToStringReturnsEmptyForUnknown)
    {
        EXPECT_EQ(ToString(HttpField::Unknown), "");
    }

    TEST(HttpFieldTest, ToStringReturnsEmptyForCastedMaxValue)
    {
        // Cast a value beyond the last defined enum value to HttpField — should hit default case
        // NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
        EXPECT_EQ(ToString(static_cast<HttpField>(131)), "");
        // NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)
    }

    TEST(HttpFieldTest, ToStringReturnsEmptyForCastedZero)
    {
        // HttpField::Unknown == 0
        EXPECT_EQ(ToString(static_cast<HttpField>(0U)), "");
    }

    TEST(HttpFieldTest, EnumUnderlyingTypeIsUint8)
    {
        static_assert(std::is_same_v<std::underlying_type_t<HttpField>, std::uint8_t>);
    }

    TEST(HttpFieldTest, ToStringIsNodiscard)
    {
        // Verify [[nodiscard]] — compile-time check: the function returns a value
        const std::string_view result = ToString(HttpField::Host);
        EXPECT_EQ(result, "Host");
    }
}   // namespace Flux::Test
