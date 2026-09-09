#include "Server.hpp"

#include <algorithm>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>
#include <csignal>
#include <future>
#include <utility>

#include "Utils/Log.hpp"

namespace Flux
{
    Server::Server(const std::size_t threadCount, const Endpoint& endpoint)
        : _ioContextPool(std::max<std::size_t>(1, threadCount))
        , _threadCount(std::max<std::size_t>(1, threadCount))
        , _signalSet(_ioContextPool.GetStrand(), SIGINT, SIGTERM)
        , _status(Status::Initialized)
    {
        auto acceptorResult = Acceptor::CreateAcceptor(_ioContextPool.GetStrand(), endpoint);
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
                _status = Status::Stopping;
            }
            _stoppedCondition.notify_all();

            InfoLog("Shutdown requested by signal={}", signalNumber);
        });
    }

    Server::~Server()
    {
        if (_status == Status::Stopped)
            return;

        if (const auto result = Stop(); !result)
            ErrorLog("Destructor failed to stop server: {}", result.error().message());
    }

    auto Server::Run() -> Result
    {
        if (_status != Status::Initialized)
        {
            WarnLog("Server run failed: Status is not initialized");
            return std::unexpected(MakeErrorCode(Error::ServerNotInitialized));
        }

        if (const auto poolResult = _ioContextPool.Run(); !poolResult)
        {
            ErrorLog("Failed to start ioContextPool: {}\nStopping server...",
                     poolResult.error().message());

            if (const auto result = Stop(); !result)
                ErrorLog("Failed to stop server: {}", result.error().message());

            return poolResult;
        }

        auto completion = std::make_shared<std::promise<Result>>();
        auto started = completion->get_future();
        _acceptor->AsyncStartAccepting(
            [completion](Result result) -> void { completion->set_value(std::move(result)); });
        completion.reset();

        if (const auto startResult = started.get(); !startResult)
        {
            ErrorLog("Failed to start acceptor: {}\nStopping server...",
                     startResult.error().message());

            if (const auto result = Stop(); !result)
                ErrorLog("Failed to stop server: {}", result.error().message());

            return startResult;
        }

        std::unique_lock lock(_mutex);
        _stoppedCondition.wait(lock, [this] -> bool {
            return _status == Status::Stopping || _status == Status::Stopped;
        });
        return {};
    }

    auto Server::Stop() -> Result
    {
        if (_status != Status::Stopping)
        {
            WarnLog("Server stop failed: Server status is not Status::Stopping");
            return std::unexpected(MakeErrorCode(Error::ServerNotStopping));
        }

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
            auto completion = std::make_shared<std::promise<Result>>();
            auto stopped = completion->get_future();
            _acceptor->AsyncStopAccepting(
                [completion](Result result) -> void { completion->set_value(std::move(result)); });
            completion.reset();

            if (const auto stopResult = stopped.get(); !stopResult)
            {
                ErrorLog("Failed to stop acceptor");
                return stopResult;
            }
        }

        if (const auto ioContextStopResult = _ioContextPool.Stop(); !ioContextStopResult)
        {
            if (!firstError)
                firstError = ioContextStopResult.error();

            ErrorLog("Failed to stop io_context pool: {}", ioContextStopResult.error().message());
        }

        if (firstError)
            return std::unexpected(firstError);

        InfoLog("Server stopped");

        _status = Status::Stopped;

        _stoppedCondition.notify_all();

        return {};
    }
}   // namespace Flux
