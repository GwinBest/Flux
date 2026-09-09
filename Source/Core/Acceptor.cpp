#include "Acceptor.hpp"

#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>

#include "Utils/Log.hpp"

namespace Flux
{
    Acceptor::~Acceptor() noexcept
    {
        if (_status == Status::Stopped)
            return;

        if (const auto stopResult = StopAccepting(); !stopResult)
            ErrorLog("Destructor failed to stop acceptor: {}", stopResult.error().message());
    }

    auto Acceptor::CreateAcceptor(const IoContextPool::Strand& strand,
                                  const boost::asio::ip::tcp::endpoint& endpoint)
        -> std::expected<std::shared_ptr<Acceptor>, ErrorCode>
    {
        auto acceptor = std::shared_ptr<Acceptor>(new Acceptor(strand));

        if (const auto openResult = acceptor->Open(endpoint); !openResult)
            return std::unexpected(openResult.error());

        return acceptor;
    }

    auto Acceptor::StartAccepting() -> Result
    {
        if (_status != Status::Initialized)
        {
            WarnLog("Start rejected: acceptor is not initialized");
            return std::unexpected(MakeErrorCode(Error::AcceptorNotInitialized));
        }

        _status = Status::Started;

        InfoLog("Acceptor started accepting connections");

        DoAccept();

        return {};
    }

    auto Acceptor::StopAccepting() noexcept -> Result
    {
        if (_status != Status::Initialized && _status != Status::Started)
        {
            WarnLog("Stop rejected: acceptor is not started");
            return std::unexpected(MakeErrorCode(Error::AcceptorNotStarted));
        }

        ErrorCode firstError;

        if (const auto currentStatus = _status.load();
            currentStatus == Status::Stopped || currentStatus == Status::NotInitialized)
            return {};

        _status = Status::Stopped;

        std::size_t cancelledTimers = 0U;

        try
        {
            cancelledTimers = _retryTimer.cancel();
        }
        catch (const boost::system::system_error& error)
        {
            firstError = error.code();
            WarnLog("Failed to cancel acceptor retry timer: {}", error.code().message());
        }

        if (_acceptor.is_open())
        {
            ErrorCode errorCode;
            errorCode = _acceptor.close(errorCode);
            if (errorCode)
            {
                if (!firstError)
                    firstError = errorCode;

                ErrorLog("Failed to close acceptor: {}", errorCode.message());
            }
            else
            {
                InfoLog("Acceptor stopped; cancelled_retry_timers={}", cancelledTimers);
            }
        }
        else
        {
            DebugLog("Acceptor stop requested while socket is already closed");
        }

        if (firstError)
            return std::unexpected(firstError);

        return {};
    }

    Acceptor::Acceptor(const IoContextPool::Strand& strand)
        : _acceptor(strand)
        , _retryTimer(strand)
    {}

    auto Acceptor::Open(const boost::asio::ip::tcp::endpoint& endpoint) -> Result
    {
        if (_acceptor.is_open())
        {
            WarnLog("Acceptor open rejected: acceptor already open");
            return std::unexpected(MakeErrorCode(Error::AcceptorNotStarted));
        }

        ErrorCode errorCode;

        errorCode = _acceptor.open(endpoint.protocol(), errorCode);
        if (errorCode)
        {
            ErrorLog("Failed to open acceptor: endpoint={}:{} error={}",
                     endpoint.address().to_string(),
                     endpoint.port(),
                     errorCode.message());
            return std::unexpected(errorCode);
        }

        errorCode = _acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true),
                                         errorCode);
        if (errorCode)
        {
            ErrorLog("Failed to configure acceptor reuse_address: endpoint={}:{} error={}",
                     endpoint.address().to_string(),
                     endpoint.port(),
                     errorCode.message());
            return std::unexpected(errorCode);
        }

        errorCode = _acceptor.bind(endpoint, errorCode);
        if (errorCode)
        {
            ErrorLog("Failed to bind acceptor: endpoint={}:{} error={}",
                     endpoint.address().to_string(),
                     endpoint.port(),
                     errorCode.message());
            return std::unexpected(errorCode);
        }

        errorCode = _acceptor.listen(boost::asio::socket_base::max_listen_connections, errorCode);
        if (errorCode)
        {
            ErrorLog("Failed to listen on acceptor: endpoint={}:{} error={}",
                     endpoint.address().to_string(),
                     endpoint.port(),
                     errorCode.message());
            return std::unexpected(errorCode);
        }

        _status = Status::Initialized;

        InfoLog("Acceptor listening: endpoint={}:{}",
                endpoint.address().to_string(),
                endpoint.port());

        return {};
    }

    auto Acceptor::DoAccept() -> void
    {
        if (_status != Status::Started || !_acceptor.is_open())
            return;

        const auto weakSelf = weak_from_this();

        _acceptor.async_accept([weakSelf](const boost::system::error_code& errorCode,
                                          boost::asio::ip::tcp::socket acceptedSocket) -> void {
            const auto self = weakSelf.lock();

            if (!self)
                return;

            if (!errorCode)
            {
                boost::system::error_code remoteEndpointError;
                const auto remoteEndpoint = acceptedSocket.remote_endpoint(remoteEndpointError);

                if (remoteEndpointError)
                {
                    InfoLog("Accepted client connection; remote endpoint unavailable: {}",
                            remoteEndpointError.message());
                }
                else
                {
                    InfoLog("Accepted client connection: remote={}:{}",
                            remoteEndpoint.address().to_string(),
                            remoteEndpoint.port());
                }

                // TODO: handle client
            }
            else if (errorCode == boost::asio::error::operation_aborted)
            {
                DebugLog("Acceptor async accept cancelled");
                return;
            }
            else
            {
                WarnLog("Acceptor async accept failed: {}", errorCode.message());

                self->_retryTimer.expires_after(RetryInterval);

                self->_retryTimer.async_wait(
                    [self](const boost::system::error_code& timerError) -> void {
                        if (timerError == boost::asio::error::operation_aborted)
                            return;

                        if (timerError)
                        {
                            WarnLog("Acceptor retry timer failed: {}", timerError.message());
                            return;
                        }

                        self->DoAccept();
                    });

                return;
            }

            self->DoAccept();   // Accept next client
        });
    }
}   // namespace Flux
