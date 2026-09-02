#include "Acceptor.hpp"

#include <boost/asio/error.hpp>
#include <spdlog/spdlog.h>

namespace Flux
{
    Acceptor::~Acceptor() noexcept
    {
        if (const auto error = StopAccepting())
            spdlog::error("Failed to stop acceptor from destructor: {}", error.message());
    }

    auto Acceptor::CreateAcceptor(const IoContextPool::Strand& strand,
                                  const boost::asio::ip::tcp::endpoint& endpoint)
        -> std::shared_ptr<Acceptor>
    {
        return std::shared_ptr<Acceptor>(new Acceptor(strand, endpoint));
    }

    auto Acceptor::StartAccepting() -> std::expected<bool, std::string>
    {
        const std::scoped_lock lock(_mutex);

        if (_status != Status::Initialized)
        {
            spdlog::warn("Acceptor start rejected: status is not Initialized");
            return std::unexpected(
                "Acceptor::StartAccepting(): Acceptor status is not Status::Initialized");
        }

        _status = Status::Started;
        spdlog::info("Acceptor started accepting connections");

        DoAccept();

        return true;
    }

    auto Acceptor::StopAccepting() noexcept -> boost::system::error_code
    {
        const std::scoped_lock lock(_mutex);

        _status = Status::Stopped;

        boost::system::error_code errorCode;

        _retryTimer.cancel();

        if (_acceptor.is_open())
        {
            errorCode = _acceptor.close(errorCode);
            if (errorCode)
                spdlog::error("Failed to stop acceptor: {}", errorCode.message());
            else
                spdlog::info("Acceptor stopped");
        }
        else
        {
            spdlog::debug("Acceptor stop requested while socket is already closed");
        }

        return errorCode;
    }

    Acceptor::Acceptor(const IoContextPool::Strand& strand,
                       const boost::asio::ip::tcp::endpoint& endpoint)
        : _acceptor(strand)
        , _retryTimer(strand)
        , _status(Status::Initialized)
    {
        _acceptor.open(endpoint.protocol());
        _acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        _acceptor.bind(endpoint);
        _acceptor.listen();

        spdlog::info("Acceptor is listening on {}:{}",
                     endpoint.address().to_string(),
                     endpoint.port());
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
                    spdlog::info("Accepted client connection");
                else
                    spdlog::info("Accepted client connection from {}:{}",
                                 remoteEndpoint.address().to_string(),
                                 remoteEndpoint.port());

                // TODO: handle client
            }
            else if (errorCode == boost::asio::error::operation_aborted)
            {
                spdlog::debug("Acceptor async accept cancelled");
                return;
            }
            else
            {
                spdlog::warn("Acceptor async accept failed: {}", errorCode.message());

                self->_retryTimer.expires_after(RetryInterval);

                self->_retryTimer.async_wait(
                    [self](const boost::system::error_code& timerError) -> void {
                        if (timerError == boost::asio::error::operation_aborted)
                            return;

                        if (timerError)
                        {
                            spdlog::warn("Acceptor::DoAccept(): Retry timer error {}",
                                         timerError.message());
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
