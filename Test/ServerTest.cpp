#include "Server.hpp"
#include "gtest/gtest.h"

#include <boost/asio/error.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <expected>
#include <future>
#include <string>
#include <thread>
#include <type_traits>

namespace Flux::Test
{
    static_assert(!std::is_copy_constructible_v<Server>);
    static_assert(!std::is_copy_assignable_v<Server>);
    static_assert(!std::is_move_constructible_v<Server>);
    static_assert(!std::is_move_assignable_v<Server>);
    static_assert(std::is_destructible_v<Server>);

}   // namespace Flux::Test
