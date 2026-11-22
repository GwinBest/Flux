#pragma once

#include <cstdint>
#include <string_view>

namespace Flux
{
    enum class HttpField : std::uint8_t
    {
        Unknown,

        Accept,
        AcceptCH,
        AcceptCharset,   // Deprecated
        AcceptEncoding,
        AcceptLanguage,
        AcceptPatch,
        AcceptPost,
        AcceptRanges,
        AcceptSignature,
        AccessControlAllowCredentials,
        AccessControlAllowHeaders,
        AccessControlAllowMethods,
        AccessControlAllowOrigin,
        AccessControlExposeHeaders,
        AccessControlMaxAge,
        AccessControlRequestHeaders,
        AccessControlRequestMethod,
        Age,
        Allow,
        AltSvc,
        AltUsed,
        Authorization,
        CacheControl,
        ClearSiteData,
        Connection,
        ContentDigest,
        ContentDisposition,
        ContentDPR,   // Non-standard, Deprecated
        ContentEncoding,
        ContentLanguage,
        ContentLength,
        ContentLocation,
        ContentRange,
        ContentSecurityPolicy,
        ContentSecurityPolicyReportOnly,
        ContentType,
        Cookie,
        CrossOriginEmbedderPolicy,
        CrossOriginOpenerPolicy,
        CrossOriginResourcePolicy,
        Date,
        Deprecation,
        DeviceMemory,
        Digest,
        DNT,             // Non-standard, Deprecated
        DPR,             // Non-standard, Deprecated
        ETag,
        Expect,
        ExpectCT,        // Deprecated
        Expires,
        Forwarded,
        From,
        Host,
        HTTP2Settings,   // Deprecated
        IfMatch,
        IfModifiedSince,
        IfNoneMatch,
        IfRange,
        IfUnmodifiedSince,
        KeepAlive,
        LastModified,
        Link,
        Location,
        MaxForwards,
        Origin,
        OriginAgentCluster,
        Pragma,   // Deprecated
        Prefer,
        PreferenceApplied,
        Priority,
        ProxyAuthenticate,
        ProxyAuthorization,
        ProxyConnection,
        Range,
        Referer,
        ReferrerPolicy,
        Refresh,
        ReportTo,             // Non-standard, Deprecated
        ReportingEndpoints,
        ReprDigest,
        RetryAfter,
        SecCHUAFullVersion,   // Deprecated
        SecFetchDest,
        SecFetchMode,
        SecFetchSite,
        SecFetchUser,
        SecPurpose,
        SecWebSocketAccept,
        SecWebSocketExtensions,
        SecWebSocketKey,
        SecWebSocketProtocol,
        SecWebSocketVersion,
        Server,
        ServerTiming,
        ServiceWorker,
        ServiceWorkerAllowed,
        ServiceWorkerNavigationPreload,
        SetCookie,
        SetLogin,
        Signature,
        SignatureInput,
        Sourcemap,
        StrictTransportSecurity,
        TE,
        TimingAllowOrigin,
        TK,   // Non-standard, Deprecated
        Trailer,
        TransferEncoding,
        Upgrade,
        UpgradeInsecureRequests,
        UserAgent,
        Vary,
        Via,
        ViewportWidth,                   // Non-standard, Deprecated
        WantContentDigest,
        WantReprDigest,
        Warning,                         // Deprecated
        Width,                           // Non-standard, Deprecated
        WwwAuthenticate,
        XContentTypeOptions,
        XDNSPrefetchControl,             // Non-standard
        XForwardedFor,                   // Non-standard
        XForwardedHost,                  // Non-standard
        XForwardedProto,                 // Non-standard
        XFrameOptions,
        XPermittedCrossDomainPolicies,   // Non-standard
        XPoweredBy,                      // Non-standard
        XRobotsTag,                      // Non-standard
        XXSSProtection,                  // Non-standard, Deprecated
    };

    [[nodiscard]] auto ToString(const HttpField field) -> std::string_view;
}   // namespace Flux
