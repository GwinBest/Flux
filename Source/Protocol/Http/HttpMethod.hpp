#pragma once

#include <cstdint>
#include <string_view>

namespace Flux
{
    enum class HttpMethod : std::uint8_t
    {
        Unknown,

        Delete,
        Get,
        Head,
        Post,
        Put,
        Connect,
        Options,
        Trace,

        // WebDAV
        Copy,
        Lock,
        MkCol,
        Move,
        Propfind,
        Proppatch,
        Search,
        Unlock,
        Bind,
        Rebind,
        Unbind,
        Acl,

        // Subversion
        Report,
        MkActivity,
        Checkout,
        Merge,

        // UPnP
        MSearch,
        Notify,
        Subscribe,
        Unsubscribe,

        // RFC-5789
        Patch,
        Purge,

        // CalDAV
        MkCalendar,

        // RFC-2068, section 19.6.1.2
        Link,
        Unlink,
    };

    [[nodiscard]] auto ToString(const HttpMethod method) -> std::string_view;

    [[nodiscard]] auto FromString(const std::string_view method) -> HttpMethod;
}   // namespace Flux
