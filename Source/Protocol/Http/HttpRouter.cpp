#include "HttpRouter.hpp"

#include <boost/beast/http/verb.hpp>
#include <utility>

#include "Protocol/Http/HttpRequest.hpp"
#include "Protocol/Http/HttpResponse.hpp"
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
                    ToString(key.Method),
                    key.Path);
        }

        return inserted;
    }

    auto HttpRouter::MatchRoute(const HttpRequest& request, HttpResponse& response) const -> bool
    {
        const HttpRouteKey key {
            .Method = request.Method(),
            .Path = std::string(request.Path()),
        };

        if (const auto it = _route.find(key); it != _route.end())   // NOLINT(*-identifier-length)
        {
            it->second(request, response);
            return true;
        }

        return false;
    }
}   // namespace Flux
