#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <unordered_map>
#include <utility>

namespace Flux
{
    using HttpMethod = boost::beast::http::verb;

    using HttpRequest = boost::beast::http::request<boost::beast::http::string_body>;
    using HttpResponse = boost::beast::http::response<boost::beast::http::string_body>;

    using HttpHandler = std::function<void(const HttpRequest& request, HttpResponse& response)>;

    struct HttpRouteKey
    {
        auto operator==(const HttpRouteKey& other) const noexcept -> bool
        {
            return Method == other.Method && Path == other.Path;
        }

        HttpMethod Method;
        std::string Path;
    };

    struct RouteKeyHash
    {
        auto operator()(const HttpRouteKey& key) const noexcept -> std::size_t
        {
            using UnderlyingType = std::underlying_type_t<HttpMethod>;

            const auto hash1 = std::hash<UnderlyingType> {}(std::to_underlying(key.Method));
            const auto hash2 = std::hash<std::string> {}(key.Path);

            return hash1 ^ (hash2 << 1U);
        }
    };

    class HttpRouter
    {
    public:
        [[nodiscard]] auto AddRoute(const HttpRouteKey& key, const HttpHandler& handler) -> bool;

        [[nodiscard]] auto MatchRoute(const HttpRequest& request, HttpResponse& response) const
            -> bool;

    private:
        std::unordered_map<HttpRouteKey, HttpHandler, RouteKeyHash> _route;
    };

}   // namespace Flux
