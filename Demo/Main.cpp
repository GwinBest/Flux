#include <cstdlib>
#include <iostream>

#include "Server.hpp"

auto main() -> int
{
    constexpr unsigned short port = 8081;
    Flux::Server server(std::thread::hardware_concurrency(),
                        Flux::Endpoint(boost::asio::ip::address_v4::loopback(), port));

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
