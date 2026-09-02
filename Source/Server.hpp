#pragma once

#include <atomic>
#include <boost/asio/signal_set.hpp>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

#include "Core/Acceptor.hpp"
#include "Core/IoContextPool.hpp"
#include "Utils/Error.hpp"

namespace Flux
{
    using Endpoint = boost::asio::ip::tcp::endpoint;

    class Server final
    {
    public:
        Server(const std::size_t threadCount, const Endpoint& endpoint);

        Server(const Server&) = delete;
        auto operator=(const Server&) -> Server& = delete;
        Server(Server&&) = delete;
        auto operator=(Server&&) -> Server& = delete;

        ~Server();

        auto Run() -> Result;
        auto Stop() -> Result;

        [[nodiscard]] auto GetThreadCount() const noexcept -> std::size_t { return _threadCount; }

    private:
        enum class Status : std::uint8_t
        {
            NotInitialized,
            Initialized,
            Running,
            Stopping,
            Stopped,
        };

    private:
        auto RequestStop() -> void;
        auto Shutdown() -> Result;

    private:
        IoContextPool _ioContextPool;
        std::shared_ptr<Acceptor> _acceptor;

        std::size_t _threadCount;

        boost::asio::signal_set _signalSet;

        std::mutex _mutex;
        std::condition_variable _stoppedCondition;

        std::atomic<Status> _status;
    };

}   // namespace Flux
