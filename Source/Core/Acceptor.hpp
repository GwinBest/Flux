#pragma once

#include <atomic>
#include <boost/asio/ip/tcp.hpp>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>

#include "../Utils/Error.hpp"
#include "IoContextPool.hpp"

namespace Flux
{
    class Acceptor final : public std::enable_shared_from_this<Acceptor>
    {
    public:
        Acceptor(const Acceptor&) = delete;
        auto operator=(const Acceptor&) -> Acceptor& = delete;
        Acceptor(Acceptor&&) = delete;
        auto operator=(Acceptor&&) -> Acceptor& = delete;

        ~Acceptor() noexcept;

        static auto CreateAcceptor(const IoContextPool::Strand& strand,
                                   const boost::asio::ip::tcp::endpoint& endpoint)
            -> std::expected<std::shared_ptr<Acceptor>, ErrorCode>;

        auto StartAccepting() -> Result;
        auto StopAccepting() noexcept -> Result;

    private:
        enum class Status : std::uint8_t
        {
            NotInitialized,
            Initialized,
            Started,
            Stopped,
        };

    private:
        explicit Acceptor(const IoContextPool::Strand& strand);

        auto Open(const boost::asio::ip::tcp::endpoint& endpoint) noexcept -> Result;

        auto DoAccept() -> void;

    private:
        boost::asio::ip::tcp::acceptor _acceptor;

        boost::asio::steady_timer _retryTimer;
        static constexpr auto RetryInterval = std::chrono::milliseconds(100);

        std::atomic<Status> _status = Status::NotInitialized;
    };

}   // namespace Flux
