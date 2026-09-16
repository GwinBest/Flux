#pragma once

#include <array>
#include <bit>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/system_error.hpp>

namespace Flux::Test
{
    inline auto OccupyEndpoint(boost::asio::ip::tcp::acceptor& acceptor) -> void
    {
        using Tcp = boost::asio::ip::tcp;
        acceptor.open(Tcp::v4());
#ifdef _WIN32
        // SO_REUSEADDR permits a second bind on Windows unless the first socket
        // explicitly requests exclusive ownership before binding.
        constexpr auto exclusive = std::bit_cast<std::array<char, sizeof(int)>>(1);
        if (setsockopt(acceptor.native_handle(),
                       SOL_SOCKET,
                       SO_EXCLUSIVEADDRUSE,
                       exclusive.data(),
                       static_cast<int>(exclusive.size()))
            != 0)
        {
            throw boost::system::system_error(WSAGetLastError(), boost::system::system_category());
        }
#endif
        acceptor.bind({boost::asio::ip::address_v4::loopback(), 0});
        acceptor.listen();
    }
}   // namespace Flux::Test

f
