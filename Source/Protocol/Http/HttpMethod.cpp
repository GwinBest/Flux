#include "HttpMethod.hpp"

namespace Flux
{
    auto ToString(const HttpMethod method) -> std::string_view
    {
        switch (method)
        {
        case HttpMethod::Unknown:     return "";
        case HttpMethod::Delete:      return "DELETE";
        case HttpMethod::Get:         return "GET";
        case HttpMethod::Head:        return "HEAD";
        case HttpMethod::Post:        return "POST";
        case HttpMethod::Put:         return "PUT";
        case HttpMethod::Connect:     return "CONNECT";
        case HttpMethod::Options:     return "OPTIONS";
        case HttpMethod::Trace:       return "TRACE";
        case HttpMethod::Copy:        return "COPY";
        case HttpMethod::Lock:        return "LOCK";
        case HttpMethod::MkCol:       return "MKCOL";
        case HttpMethod::Move:        return "MOVE";
        case HttpMethod::Propfind:    return "PROPFIND";
        case HttpMethod::Proppatch:   return "PROPPATCH";
        case HttpMethod::Search:      return "SEARCH";
        case HttpMethod::Unlock:      return "UNLOCK";
        case HttpMethod::Bind:        return "BIND";
        case HttpMethod::Rebind:      return "REBIND";
        case HttpMethod::Unbind:      return "UNBIND";
        case HttpMethod::Acl:         return "ACL";
        case HttpMethod::Report:      return "REPORT";
        case HttpMethod::MkActivity:  return "MKACTIVITY";
        case HttpMethod::Checkout:    return "CHECKOUT";
        case HttpMethod::Merge:       return "MERGE";
        case HttpMethod::MSearch:     return "M-SEARCH";
        case HttpMethod::Notify:      return "NOTIFY";
        case HttpMethod::Subscribe:   return "SUBSCRIBE";
        case HttpMethod::Unsubscribe: return "UNSUBSCRIBE";
        case HttpMethod::Patch:       return "PATCH";
        case HttpMethod::Purge:       return "PURGE";
        case HttpMethod::MkCalendar:  return "MKCALENDAR";
        case HttpMethod::Link:        return "LINK";
        case HttpMethod::Unlink:      return "UNLINK";
        default:                      return "UNKNOWN";
        }
    }

    auto FromString(const std::string_view method)   // NOLINT(*-function-cognitive-complexity)
        -> HttpMethod
    {
        if (method == "DELETE")
            return HttpMethod::Delete;
        if (method == "GET")
            return HttpMethod::Get;
        if (method == "HEAD")
            return HttpMethod::Head;
        if (method == "POST")
            return HttpMethod::Post;
        if (method == "PUT")
            return HttpMethod::Put;
        if (method == "CONNECT")
            return HttpMethod::Connect;
        if (method == "OPTIONS")
            return HttpMethod::Options;
        if (method == "TRACE")
            return HttpMethod::Trace;
        if (method == "COPY")
            return HttpMethod::Copy;
        if (method == "LOCK")
            return HttpMethod::Lock;
        if (method == "MKCOL")
            return HttpMethod::MkCol;
        if (method == "MOVE")
            return HttpMethod::Move;
        if (method == "PROPFIND")
            return HttpMethod::Propfind;
        if (method == "PROPPATCH")
            return HttpMethod::Proppatch;
        if (method == "SEARCH")
            return HttpMethod::Search;
        if (method == "UNLOCK")
            return HttpMethod::Unlock;
        if (method == "BIND")
            return HttpMethod::Bind;
        if (method == "REBIND")
            return HttpMethod::Rebind;
        if (method == "UNBIND")
            return HttpMethod::Unbind;
        if (method == "ACL")
            return HttpMethod::Acl;
        if (method == "REPORT")
            return HttpMethod::Report;
        if (method == "MKACTIVITY")
            return HttpMethod::MkActivity;
        if (method == "CHECKOUT")
            return HttpMethod::Checkout;
        if (method == "MERGE")
            return HttpMethod::Merge;
        if (method == "M-SEARCH")
            return HttpMethod::MSearch;
        if (method == "NOTIFY")
            return HttpMethod::Notify;
        if (method == "SUBSCRIBE")
            return HttpMethod::Subscribe;
        if (method == "UNSUBSCRIBE")
            return HttpMethod::Unsubscribe;
        if (method == "PATCH")
            return HttpMethod::Patch;
        if (method == "PURGE")
            return HttpMethod::Purge;
        if (method == "MKCALENDAR")
            return HttpMethod::MkCalendar;
        if (method == "LINK")
            return HttpMethod::Link;
        if (method == "UNLINK")
            return HttpMethod::Unlink;
        return HttpMethod::Unknown;
    }
}   // namespace Flux
