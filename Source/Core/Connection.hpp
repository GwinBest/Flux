#pragma once

#include <array>
#include <atomic>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
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
        static constexpr auto IdleTimeoutDuration = std::chrono::seconds(120);

    public:
        auto AsyncStart() -> std::future<Result>;
        auto AsyncClose() -> std::future<Result>;

        auto Send(std::vector<std::byte>&& data) -> std::future<Result>;

        auto SetMaxRequestSize(const std::size_t maxRequestSize) -> void;
        [[nodiscard]] auto GetMaxRequestSize() const -> std::size_t;

        auto SetIdleTimeout(const std::chrono::seconds idleTimeout) -> void;
        [[nodiscard]] auto GetIdleTimeout() const -> std::chrono::seconds;

    private:
        enum class Status : std::uint8_t
        {
            NotInitialized,
            Started,
            Closed,
        };

    private:
        Connection(boost::asio::ip::tcp::socket&& socket,
                   std::function<void(const std::shared_ptr<Connection>&)>&& onClose,
                   const std::chrono::seconds idleTimeout = IdleTimeoutDuration);

        auto Start() -> Result;
        auto Close() -> Result;

        auto QueueSend(std::vector<std::byte>&& data) -> Result;

        auto DoRead() -> Result;
        auto DoWrite() -> Result;

        auto RestartIdleTimer() -> void;

    private:
        boost::asio::ip::tcp::socket _socket;

        static constexpr std::uint16_t BufferSize = 1024 * 8;
        std::array<std::byte, BufferSize> _readBuffer;

        static constexpr std::size_t DefaultMaxRequestSize = BufferSize * 4ULL;
        std::atomic<std::size_t> _maxRequestSize;
        std::vector<std::byte> _incomingBuffer;

        std::deque<std::shared_ptr<std::vector<std::byte>>> _sendQueue;
        std::size_t _writeOffset = 0;

        std::function<void(const std::shared_ptr<Connection>&)> _onClose;

        std::atomic<std::chrono::seconds> _idleTimeout;
        boost::asio::steady_timer _idleTimer;
        std::uint64_t _idleTimerGeneration = 0;

        Status _status = Status::NotInitialized;
    };

}   // namespace Flux
