#pragma once

#include <array>
#include <boost/asio/ip/tcp.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <utility>
#include <vector>

#include "Utils/Error.hpp"

namespace Flux
{
    class Connection final : public std::enable_shared_from_this<Connection>
    {
        friend class ConnectionManager;

    public:
        auto AsyncStart() -> std::future<Result>;
        auto AsyncClose() -> std::future<Result>;

        auto Send(std::vector<std::byte>&& data) -> std::future<Result>;

    private:
        enum class Status : std::uint8_t
        {
            NotInitialized,
            Started,
            Closed,
        };

    private:
        Connection(boost::asio::ip::tcp::socket&& socket,
                   std::function<void(const std::shared_ptr<Connection>&)>&& onClose)
            : _socket(std::move(socket))
            , _readBuffer()
            , _onClose(std::move(onClose))
        {}

        auto Start() noexcept -> Result;
        auto Close() noexcept -> Result;

        auto QueueSend(std::vector<std::byte>&& data) noexcept -> Result;

        auto DoRead() noexcept -> Result;
        auto DoWrite() noexcept -> Result;

    private:
        boost::asio::ip::tcp::socket _socket;

        static constexpr std::uint16_t BufferSize = 1024 * 8;
        std::array<std::byte, BufferSize> _readBuffer;

        std::deque<std::shared_ptr<std::vector<std::byte>>> _sendQueue;

        std::function<void(const std::shared_ptr<Connection>&)> _onClose;

        Status _status = Status::NotInitialized;
    };

}   // namespace Flux
