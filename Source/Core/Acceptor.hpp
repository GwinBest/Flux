#pragma once

#include <atomic>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>

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
            -> std::shared_ptr<Acceptor>;

        auto StartAccepting() -> std::expected<bool, std::string>;
        auto StopAccepting() noexcept -> boost::system::error_code;

    private:
        enum class Status : std::uint8_t
        {
            NotInitialized,
            Initialized,
            Started,
            Stopped,
        };

    private:
        Acceptor(const IoContextPool::Strand& strand,
                 const boost::asio::ip::tcp::endpoint& endpoint);

        auto DoAccept() -> void;

    private:
        boost::asio::ip::tcp::acceptor _acceptor;

        std::mutex _mutex;

        boost::asio::steady_timer _retryTimer;
        static constexpr auto RetryInterval = std::chrono::milliseconds(100);

        std::atomic<Status> _status = Status::NotInitialized;
    };

}   // namespace Flux
