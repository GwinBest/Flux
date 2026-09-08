#include "Server.hpp"

auto main() -> int
{
    Flux::Server server(std::thread::hardware_concurrency(),
                        Flux::Endpoint(boost::asio::ip::address_v4::loopback(), 8081));

    server.Run();
    server.Stop();
    return 0;
}
