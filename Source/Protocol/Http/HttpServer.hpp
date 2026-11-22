#pragma once

#include <memory>
#include <string>

#include "Protocol/Http/HttpProtocolSession.hpp"
#include "Protocol/Http/HttpRouter.hpp"
#include "Server/Server.hpp"

namespace Flux
{
    class HttpServer final
    {
    public:
        explicit HttpServer(const ServerOptions& options = ServerOptions());

        HttpServer(const HttpServer&) = delete;
        auto operator=(const HttpServer&) -> HttpServer& = delete;
        HttpServer(HttpServer&&) = delete;
        auto operator=(HttpServer&&) -> HttpServer& = delete;

        ~HttpServer() = default;

        [[nodiscard]] auto AddRoute(const HttpRouteKey& key, const HttpHandler& handler) const
            -> bool;

        [[nodiscard]] auto Get(const std::string& path, const HttpHandler& handler) const -> bool;
        [[nodiscard]] auto Post(const std::string& path, const HttpHandler& handler) const -> bool;
        [[nodiscard]] auto Put(const std::string& path, const HttpHandler& handler) const -> bool;
        [[nodiscard]] auto Delete(const std::string& path, const HttpHandler& handler) const
            -> bool;
        [[nodiscard]] auto Patch(const std::string& path, const HttpHandler& handler) const -> bool;
        [[nodiscard]] auto Head(const std::string& path, const HttpHandler& handler) const -> bool;
        [[nodiscard]] auto Options(const std::string& path, const HttpHandler& handler) const
            -> bool;

        [[nodiscard]] auto Router() const noexcept -> std::shared_ptr<HttpRouter>;

        [[nodiscard]] auto Run() const -> Result;
        [[nodiscard]] auto Stop() const -> Result;

        [[nodiscard]] auto GetThreadCount() const noexcept -> std::size_t;
        [[nodiscard]] auto GetMaxConnections() const noexcept -> std::size_t;
        [[nodiscard]] auto GetOptions() const noexcept -> const ServerOptions&;

    private:
        std::shared_ptr<HttpRouter> _router;
        std::unique_ptr<Server> _server;
    };

}   // namespace Flux
