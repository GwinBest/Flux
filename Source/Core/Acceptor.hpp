#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <cstdint>
#include <expected>
#include <future>
#include <memory>

#include "Core/IoContextPool.hpp"
#include "Utils/Error.hpp"

namespace Flux
{
    class Acceptor final : public std::enable_shared_from_this<Acceptor>
    {
    public:
        Acceptor(const Acceptor&) = delete;
        auto operator=(const Acceptor&) -> Acceptor& = delete;
        Acceptor(Acceptor&&) = delete;
        auto operator=(Acceptor&&) -> Acceptor& = delete;

        ~Acceptor() = default;

        static auto CreateAcceptor(const IoContextPool::Strand& strand,
                                   const boost::asio::ip::tcp::endpoint& endpoint)
            -> std::expected<std::shared_ptr<Acceptor>, ErrorCode>;

        auto AsyncStartAccepting() -> std::future<Result>;
        auto AsyncStopAccepting() -> std::future<Result>;

    private:
        enum class Status : std::uint8_t
        {
            NotInitialized,
            Initialized,
            Started,
            Stopped,
        };

    private:
        explicit Acceptor(IoContextPool::Strand strand);

        auto Open(const boost::asio::ip::tcp::endpoint& endpoint) -> Result;

        auto StartAccepting() -> Result;
        auto StopAccepting() -> Result;

        auto DoAccept() -> void;
        auto OnAccept(const ErrorCode& errorCode, boost::asio::ip::tcp::socket acceptedSocket)
            -> void;

    private:
        IoContextPool::Strand _strand;
        boost::asio::ip::tcp::acceptor _acceptor;

        boost::asio::steady_timer _retryTimer;
        static constexpr auto RetryInterval = std::chrono::milliseconds(100);

        Status _status = Status::NotInitialized;
    };

}   // namespace Flux
