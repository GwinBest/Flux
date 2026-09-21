#pragma once

#include <array>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

#include "Utils/Error.hpp"

namespace Flux
{
    class Connection final : public std::enable_shared_from_this<Connection>
    {
    public:
        friend class ConnectionManager;

    public:
        using CompletionHandler = std::function<void(Result)>;

        auto StartAsync(CompletionHandler onStarted = {}) -> Result;

        auto SendAsync(std::vector<std::byte> data, CompletionHandler onQueued = {}) -> Result;

    private:
        enum class State : std::uint8_t
        {
            NotStarted,
            Started,
            Closed,
        };

    private:
        explicit Connection(boost::asio::ip::tcp::socket socket)
            : _socket(std::move(socket))
            , _strand(boost::asio::make_strand(_socket.get_executor()))
        {}

    private:
        auto Start() -> Result;

        auto Send(std::vector<std::byte> data) -> Result;

        auto DoRead() -> Result;
        auto DoWrite() -> Result;

        auto CloseAsync(CompletionHandler onClosed = {}) -> Result;
        auto Close() -> Result;

    private:
        boost::asio::ip::tcp::socket _socket;
        boost::asio::strand<boost::asio::ip::tcp::socket::executor_type> _strand;

        State _state = State::NotStarted;

        constexpr static std::uint16_t ReadBufferSize = 8192;

        std::array<std::byte, ReadBufferSize> _readBuffer {};
        std::deque<std::shared_ptr<std::vector<std::byte>>> _writeQueue;
    };
}   // namespace Flux
