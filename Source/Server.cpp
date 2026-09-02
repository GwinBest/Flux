#include "Server.hpp"

#include <algorithm>
#include <boost/asio/error.hpp>
#include <csignal>

#include "Utils/Log.hpp"

namespace Flux
{
    Server::Server(const std::size_t threadCount, const Endpoint& endpoint)
        : _ioContextPool(std::max<std::size_t>(1, threadCount))
        , _acceptor(Acceptor::CreateAcceptor(_ioContextPool.GetStrand(), endpoint).value())
        , _threadCount(std::max<std::size_t>(1, threadCount))
        , _signalSet(_ioContextPool.GetStrand(), SIGINT, SIGTERM)
        , _status(Status::NotInitialized)
    {
        _signalSet.async_wait([this](const ErrorCode& signalError, int signalNumber) -> void {
            if (signalError == boost::asio::error::operation_aborted)
            {
                Log::DebugLog("Server signal wait cancelled");
                return;
            }

            if (signalError)
            {
                Log::WarnLog(Log::Format("Server signal wait failed: {}", signalError.message()));
                return;
            }

            Log::InfoLog(Log::Format("Shutdown requested by signal={}", signalNumber));

            _status = Status::Stopping;

            _stoppedCondition.notify_all();
        });
    }

    Server::~Server()
    {
        if (const auto result = Stop(); !result)
            Log::ErrorLog(
                Log::Format("Destructor failed to stop server: {}", result.error().message()));
    }

    auto Server::Run() -> Result
    {
        std::unique_lock lock(_mutex);

        if (_status != Status::Initialized)
            return std::unexpected(MakeErrorCode(Error::ServerNotInitialized));

        auto result = _acceptor->StartAccepting();

        if (!result)
            return result;

        result = _ioContextPool.Run();

        if (!result)
        {
            if (const auto acceptorStopResult = _acceptor->StopAccepting(); !acceptorStopResult)
                Log::ErrorLog(
                    Log::Format("Failed to stop acceptor after io_context start error: {}",
                                acceptorStopResult.error().message()));

            _status = Status::Stopped;
            _stoppedCondition.notify_all();

            return result;
        }

        _status = Status::Running;

        _stoppedCondition.wait(lock, [this] -> bool { return _status == Status::Stopping; });

        return {};
    }

    auto Server::Stop() -> Result
    {
        ErrorCode firstError;

        ErrorCode signalError;
        signalError = _signalSet.cancel(signalError);
        if (signalError)
        {
            firstError = signalError;
            Log::WarnLog(
                Log::Format("Failed to cancel server signal wait: {}", signalError.message()));
        }

        if (_acceptor)
        {
            if (const auto acceptorStopResult = _acceptor->StopAccepting(); !acceptorStopResult)
            {
                if (!firstError)
                    firstError = acceptorStopResult.error();

                Log::ErrorLog(Log::Format("Failed to stop acceptor: {}",
                                          acceptorStopResult.error().message()));
            }
        }

        if (const auto ioContextStopResult = _ioContextPool.Stop(); !ioContextStopResult)
        {
            if (!firstError)
                firstError = ioContextStopResult.error();

            Log::ErrorLog(Log::Format("Failed to stop io_context pool: {}",
                                      ioContextStopResult.error().message()));
        }

        if (firstError)
            return std::unexpected(firstError);

        Log::InfoLog("Server stopped");

        _status = Status::Stopped;

        _stoppedCondition.notify_all();

        return {};
    }
}   // namespace Flux
