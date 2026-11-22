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
#include <string_view>
#include <utility>
#include <vector>

#include "Protocol/IProtocolSession.hpp"
#include "Utils/Error.hpp"

namespace Flux
{
    struct ConnectionOptions final
    {
        static constexpr auto DefaultIdleTimeout = std::chrono::seconds(120);
        static constexpr std::size_t DefaultMaxRequestSize = 32LL * 1024LL;
        static constexpr std::size_t DefaultMaxSendQueue = DefaultMaxRequestSize;

        std::chrono::seconds IdleTimeout = DefaultIdleTimeout;
        std::size_t MaxRequestSize = DefaultMaxRequestSize;
        std::size_t MaxSendQueue = DefaultMaxSendQueue;
    };

    class Connection final : public std::enable_shared_from_this<Connection>
    {
        friend class ConnectionManager;

    public:
        static constexpr auto IdleTimeoutDuration = ConnectionOptions::DefaultIdleTimeout;

    public:
        auto AsyncStart() -> std::future<Result>;
        auto AsyncClose() -> std::future<Result>;

        auto Send(std::vector<std::byte>&& data) -> std::future<Result>;

        auto SetMaxRequestSize(const std::size_t maxRequestSize) -> void;
        [[nodiscard]] auto GetMaxRequestSize() const -> std::size_t;

        auto SetMaxSendQueue(const std::size_t maxSendQueue) -> void;
        [[nodiscard]] auto GetMaxSendQueue() const -> std::size_t;

        [[nodiscard]] auto SetIdleTimeout(const std::chrono::seconds idleTimeout) -> Result;
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
                   std::unique_ptr<IProtocolSession> protocol,
                   const ConnectionOptions& options);

        auto Start() -> Result;
        auto Close() -> Result;

        auto QueueSend(std::vector<std::byte>&& data) -> Result;

        auto DoRead() -> Result;
        auto OnRead(const ErrorCode& errorCode, std::size_t length) -> void;
        auto ProcessRead(std::size_t length) -> bool;
        auto CloseAfterFailure(std::string_view reason) -> void;
        auto DoWrite() -> Result;
        auto OnWrite(const std::shared_ptr<std::vector<std::byte>>& data,
                     const ErrorCode& errorCode,
                     std::size_t length) -> void;

        auto RestartIdleTimer() -> void;

    private:
        boost::asio::ip::tcp::socket _socket;

        static constexpr std::uint16_t BufferSize = 1024 * 8;
        std::array<std::byte, BufferSize> _readBuffer;

        std::atomic<std::size_t> _maxRequestSize;

        std::size_t _pendingSendBytes = 0;
        std::atomic<std::size_t> _maxSendQueue;
        std::deque<std::shared_ptr<std::vector<std::byte>>> _sendQueue;
        std::size_t _writeOffset = 0;

        std::function<void(const std::shared_ptr<Connection>&)> _onClose;

        std::atomic<std::chrono::seconds> _idleTimeout;
        boost::asio::steady_timer _idleTimer;
        std::uint64_t _idleTimerGeneration = 0;

        std::unique_ptr<IProtocolSession> _protocol;

        Status _status = Status::NotInitialized;
    };

}   // namespace Flux
