#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <unordered_map>
#include <utility>

#include "Protocol/Http/HttpMethod.hpp"

namespace Flux
{
    class HttpRequest;
    class HttpResponse;

    using HttpHandler = std::function<void(const HttpRequest& request, HttpResponse& response)>;

    struct HttpRouteKey final
    {
        auto operator==(const HttpRouteKey& other) const noexcept -> bool
        {
            return Method == other.Method && Path == other.Path;
        }

        HttpMethod Method;
        std::string Path;
    };

    struct RouteKeyHash final
    {
        auto operator()(const HttpRouteKey& key) const noexcept -> std::size_t
        {
            using UnderlyingType = std::underlying_type_t<HttpMethod>;

            const auto hash1 = std::hash<UnderlyingType> {}(std::to_underlying(key.Method));
            const auto hash2 = std::hash<std::string> {}(key.Path);

            return hash1 ^ (hash2 << 1U);
        }
    };

    class HttpRouter final
    {
    public:
        [[nodiscard]] auto AddRoute(const HttpRouteKey& key, const HttpHandler& handler) -> bool;

        [[nodiscard]] auto MatchRoute(const HttpRequest& request, HttpResponse& response) const
            -> bool;

    private:
        std::unordered_map<HttpRouteKey, HttpHandler, RouteKeyHash> _route;
    };

}   // namespace Flux
