#pragma once

#include <boost/asio/signal_set.hpp>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "Core/Acceptor.hpp"
#include "Core/IoContextPool.hpp"
#include "Core/Protocol/HttpSession.hpp"
#include "Core/Protocol/IProtocolSession.hpp"
#include "Utils/Error.hpp"

namespace Flux
{
    using Endpoint = boost::asio::ip::tcp::endpoint;

    struct ServerOptions final
    {
        std::size_t ThreadCount = std::thread::hardware_concurrency();
        Endpoint BindEndpoint;
        std::optional<std::size_t> MaxConnections;
        ConnectionOptions ConnectionConfig {};

        SessionFactory Session = [] -> std::unique_ptr<IProtocolSession> {
            return std::make_unique<HttpSession>();
        };
    };

    class Server final
    {
    public:
        explicit Server(const ServerOptions& options = ServerOptions());
        Server(const std::size_t threadCount, const Endpoint& endpoint);

        Server(const Server&) = delete;
        auto operator=(const Server&) -> Server& = delete;
        Server(Server&&) = delete;
        auto operator=(Server&&) -> Server& = delete;

        ~Server();

        auto Run() -> Result;
        auto Stop() -> Result;

        [[nodiscard]] auto GetThreadCount() const noexcept -> std::size_t { return _threadCount; }

        [[nodiscard]] auto GetMaxConnections() const noexcept -> std::size_t
        {
            return _acceptor->GetMaxConnections();
        }

        [[nodiscard]] auto GetOptions() const noexcept -> const ServerOptions& { return _options; }

    private:
        enum class Status : std::uint8_t
        {
            NotInitialized,
            Initialized,
            Starting,
            Running,
            StopRequested,
            Stopping,
            Stopped,
        };

    private:
        IoContextPool _ioContextPool;
        std::shared_ptr<Acceptor> _acceptor;

        ServerOptions _options;
        std::size_t _threadCount;

        boost::asio::signal_set _signalSet;

        std::mutex _mutex;
        std::condition_variable _stoppedCondition;

        Status _status = Status::NotInitialized;
        bool _shutdownRequested = false;
    };

}   // namespace Flux
