#include <cstdlib>
#include <iostream>

#include "Protocol/Http/HttpField.hpp"
#include "Protocol/Http/HttpRequest.hpp"
#include "Protocol/Http/HttpResponse.hpp"
#include "Protocol/Http/HttpServer.hpp"
#include "Server/Server.hpp"

auto main() -> int
{
    constexpr unsigned short port = 8081;

    Flux::ServerOptions options;
    options.ThreadCount = std::thread::hardware_concurrency();
    options.BindEndpoint = Flux::Endpoint(boost::asio::ip::address_v4::loopback(), port);

    const Flux::HttpServer server(options);

    (void)server.Get("/", [](const Flux::HttpRequest&, Flux::HttpResponse& response) -> void {
        response.Ok().Set(Flux::HttpField::Server, "Flux").Text("Hello World!");
    });

    (void)server.Get("/test", [](const Flux::HttpRequest&, Flux::HttpResponse& response) -> void {
        response.Ok().Set(Flux::HttpField::Server, "Flux").Text("Hello, test!");
    });

    if (const auto result = server.Run(); !result)
    {
        std::cerr << "Server run failed: " << result.error().message() << '\n';
        return EXIT_FAILURE;
    }

    if (const auto result = server.Stop(); !result)
    {
        std::cerr << "Server stop failed: " << result.error().message() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
