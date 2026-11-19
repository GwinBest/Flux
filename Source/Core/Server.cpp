#include "Server.hpp"

#include <algorithm>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>
#include <chrono>
#include <csignal>
#include <future>
#include <stdexcept>
#include <utility>

#include "Utils/Log.hpp"

namespace Flux
{
    namespace
    {
        auto GetValidatedThreadCount(const ServerOptions& options) -> std::size_t
        {
            if (options.ConnectionConfig.IdleTimeout <= std::chrono::seconds::zero())
                throw std::invalid_argument("IdleTimeout must be greater than zero");
            if (!options.GetProtocolFactory())
                throw std::invalid_argument("SessionFactory must not be empty");

            return std::max<std::size_t>(1, options.ThreadCount);
        }
    }   // namespace

    Server::Server(const ServerOptions& options)
        : _ioContextPool(GetValidatedThreadCount(options))
        , _options(options)
        , _threadCount(GetValidatedThreadCount(options))
        , _signalSet(_ioContextPool.GetStrand(), SIGINT, SIGTERM)
        , _status(Status::Initialized)
    {
        auto acceptorResult = Acceptor::CreateAcceptor(_ioContextPool.GetStrand(),
                                                       options.BindEndpoint,
                                                       options.MaxConnections,
                                                       options.ConnectionConfig,
                                                       options.GetProtocolFactory());
        if (!acceptorResult)
            throw boost::system::system_error(acceptorResult.error(),
                                              "Failed to create server acceptor");

        _acceptor = std::move(*acceptorResult);

        _signalSet.async_wait([this](const ErrorCode& signalError, int signalNumber) -> void {
            if (signalError == boost::asio::error::operation_aborted)
                return;

            if (signalError)
            {
                WarnLog("Server signal wait failed: {}", signalError.message());
                return;
            }

            {
                const std::scoped_lock lock(_mutex);

                if (_status == Status::Starting)
                    _shutdownRequested = true;
                else if (_status == Status::Running)
                    _status = Status::StopRequested;
            }
            _stoppedCondition.notify_all();

            InfoLog("Shutdown requested by signal={}", signalNumber);
        });
    }

    Server::~Server()
    {
        {
            const std::scoped_lock lock(_mutex);
            if (_status == Status::Stopped)
                return;
        }

        if (const auto result = Stop(); !result)
            ErrorLog("Destructor failed to stop server: {}", result.error().message());
    }

    auto Server::Run() -> Result
    {
        std::unique_lock lock(_mutex);

        if (_status != Status::Initialized)
        {
            WarnLog("Server run failed: Status is not initialized");
            return std::unexpected(MakeErrorCode(Error::ServerNotInitialized));
        }

        _status = Status::Starting;
        lock.unlock();

        if (const auto poolResult = _ioContextPool.Run(); !poolResult)
        {
            lock.lock();
            _status = Status::NotInitialized;
            lock.unlock();
            _stoppedCondition.notify_all();
            ErrorLog("Failed to start ioContextPool: {}", poolResult.error().message());

            return poolResult;
        }

        auto started = _acceptor->AsyncStartAccepting();

        if (const auto startResult = started.get(); !startResult)
        {
            lock.lock();
            _status = Status::StopRequested;
            lock.unlock();
            ErrorLog("Failed to start acceptor: {}\nStopping server...",
                     startResult.error().message());

            if (const auto result = Stop(); !result)
                ErrorLog("Failed to stop server: {}", result.error().message());

            return startResult;
        }

        lock.lock();

        _status = _shutdownRequested ? Status::StopRequested : Status::Running;

        _stoppedCondition.wait(lock, [this] -> bool {
            return _status == Status::StopRequested || _status == Status::Stopping
                || _status == Status::Stopped || _status == Status::NotInitialized;
        });
        return {};
    }

    auto Server::Stop() -> Result
    {
        {
            const std::scoped_lock lock(_mutex);
            if (_status != Status::StopRequested && _status != Status::Running)
            {
                WarnLog("Server stop rejected: not running or shutdown already in progress");
                return std::unexpected(MakeErrorCode(Error::ServerNotStopping));
            }
            _status = Status::Stopping;
        }
        _stoppedCondition.notify_all();

        ErrorCode firstError;

        ErrorCode signalError;
        signalError = _signalSet.cancel(signalError);
        if (signalError)
        {
            firstError = signalError;
            WarnLog("Failed to cancel server signal wait: {}", signalError.message());
        }

        if (_acceptor)
        {
            auto stopped = _acceptor->AsyncStopAccepting();

            if (const auto stopResult = stopped.get(); !stopResult)
            {
                ErrorLog("Failed to stop acceptor");
                if (!firstError)
                    firstError = stopResult.error();
            }
        }

        const auto ioContextStopResult = _ioContextPool.Stop();
        if (!ioContextStopResult)
        {
            if (!firstError)
                firstError = ioContextStopResult.error();

            ErrorLog("Failed to stop io_context pool: {}", ioContextStopResult.error().message());
        }

        {
            const std::scoped_lock lock(_mutex);
            _status = ioContextStopResult ? Status::Stopped : Status::NotInitialized;
        }
        _stoppedCondition.notify_all();

        if (firstError)
            return std::unexpected(firstError);

        InfoLog("Server stopped");
        return {};
    }
}   // namespace Flux
