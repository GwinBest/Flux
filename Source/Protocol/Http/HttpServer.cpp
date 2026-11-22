#include "HttpServer.hpp"

namespace Flux
{
    HttpServer::HttpServer(const ServerOptions& options)
        : _router(std::make_shared<HttpRouter>())
    {
        ServerOptions httpOptions = options;
        httpOptions.SetProtocol<HttpProtocolSession>(_router);
        _server = std::make_unique<Server>(httpOptions);
    }

    auto HttpServer::AddRoute(const HttpRouteKey& key, const HttpHandler& handler) const -> bool
    {
        return _router->AddRoute(key, handler);
    }

    auto HttpServer::Get(const std::string& path, const HttpHandler& handler) const -> bool
    {
        return AddRoute({.Method = HttpMethod::Get, .Path = path}, handler);
    }

    auto HttpServer::Post(const std::string& path, const HttpHandler& handler) const -> bool
    {
        return AddRoute({.Method = HttpMethod::Post, .Path = path}, handler);
    }

    auto HttpServer::Put(const std::string& path, const HttpHandler& handler) const -> bool
    {
        return AddRoute({.Method = HttpMethod::Put, .Path = path}, handler);
    }

    auto HttpServer::Delete(const std::string& path, const HttpHandler& handler) const -> bool
    {
        return AddRoute({.Method = HttpMethod::Delete, .Path = path}, handler);
    }

    auto HttpServer::Patch(const std::string& path, const HttpHandler& handler) const -> bool
    {
        return AddRoute({.Method = HttpMethod::Patch, .Path = path}, handler);
    }

    auto HttpServer::Head(const std::string& path, const HttpHandler& handler) const -> bool
    {
        return AddRoute({.Method = HttpMethod::Head, .Path = path}, handler);
    }

    auto HttpServer::Options(const std::string& path, const HttpHandler& handler) const -> bool
    {
        return AddRoute({.Method = HttpMethod::Options, .Path = path}, handler);
    }

    auto HttpServer::Router() const noexcept -> std::shared_ptr<HttpRouter>
    {
        return _router;
    }

    auto HttpServer::Run() const -> Result
    {
        return _server->Run();
    }

    auto HttpServer::Stop() const -> Result
    {
        return _server->Stop();
    }

    auto HttpServer::GetThreadCount() const noexcept -> std::size_t
    {
        return _server->GetThreadCount();
    }

    auto HttpServer::GetMaxConnections() const noexcept -> std::size_t
    {
        return _server->GetMaxConnections();
    }

    auto HttpServer::GetOptions() const noexcept -> const ServerOptions&
    {
        return _server->GetOptions();
    }
}   // namespace Flux
