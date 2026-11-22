#pragma once

#include <boost/asio/signal_set.hpp>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "Networking/Acceptor.hpp"
#include "Networking/IoContextPool.hpp"
#include "Protocol/IProtocolSession.hpp"
#include "Utils/Error.hpp"

namespace Flux
{
    using Endpoint = boost::asio::ip::tcp::endpoint;

    struct ServerOptions final
    {
        template<typename Protocol, typename... Args>
        void SetProtocol(Args&&... args)
        {
            auto tupleArgs = std::make_tuple(std::forward<Args>(args)...);
            _sessionFactory =
                [tuple = std::move(tupleArgs)] mutable -> std::unique_ptr<IProtocolSession> {
                return std::apply(
                    []<typename... T>(T&&... unpacked)   // NOLINT(*-missing-std-forward)
                    -> std::unique_ptr<IProtocolSession> {
                        return std::make_unique<Protocol>(std::forward<T>(unpacked)...);
                    },
                    tuple);
            };
        }

        [[nodiscard]] auto GetProtocolFactory() const noexcept -> const SessionFactory&
        {
            return _sessionFactory;
        }

        [[nodiscard]] auto GetProtocol() const -> std::unique_ptr<IProtocolSession>
        {
            if (!_sessionFactory)
                return nullptr;
            return _sessionFactory();
        }

    public:
        std::size_t ThreadCount = std::thread::hardware_concurrency();
        Endpoint BindEndpoint;
        std::optional<std::size_t> MaxConnections;
        ConnectionOptions ConnectionConfig = {};

    private:
        SessionFactory _sessionFactory;
    };

    class Server final
    {
    public:
        explicit Server(const ServerOptions& options = ServerOptions());

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
