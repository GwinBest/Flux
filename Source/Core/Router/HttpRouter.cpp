#include "HttpRouter.hpp"

#include <utility>

#include "Core/Protocol/HttpProtocolSession.hpp"
#include "Utils/Log.hpp"

namespace Flux
{
    auto HttpRouter::AddRoute(const HttpRouteKey& key, const HttpHandler& handler) -> bool
    {
        auto [it, inserted] = _route.try_emplace(   // NOLINT(*-identifier-length)
            key,
            handler);

        if (!inserted)
        {
            WarnLog("Route conflict: {} '{}' is already registered!",
                    boost::beast::http::to_string(key.Method),
                    key.Path);
        }

        return inserted;
    }

    auto HttpRouter::MatchRoute(const HttpRequest& request, HttpResponse& response) const -> bool
    {
        const HttpRouteKey key {.Method = request.method(), .Path = std::string(request.target())};

        if (const auto it = _route.find(key); it != _route.end())   // NOLINT(*-identifier-length)
        {
            it->second(request, response);
            return true;
        }

        return false;
    }
}   // namespace Flux
