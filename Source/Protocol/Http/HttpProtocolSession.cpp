#include "HttpProtocolSession.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "Protocol/Http/HttpRequest.hpp"
#include "Protocol/Http/HttpResponse.hpp"
#include "Utils/Log.hpp"

namespace
{
    auto SerializeResponse(const Flux::Detail::HttpResponse& response) -> std::vector<std::byte>
    {
        boost::beast::http::response_serializer<Flux::Detail::HttpResponse::body_type> serializer {
            response};

        boost::beast::flat_buffer buffer;

        while (!serializer.is_done())
        {
            boost::system::error_code errorCode;
            serializer.next(errorCode, [&](auto&, const auto& buffers) -> auto {
                const auto size = boost::asio::buffer_size(buffers);
                const auto dest = buffer.prepare(size);
                const auto count = boost::asio::buffer_copy(dest, buffers);

                buffer.commit(count);

                serializer.consume(count);

                return count;
            });

            if (errorCode)
                break;
        }

        const auto constBuffers = buffer.data();
        std::vector<std::byte> bytes(boost::asio::buffer_size(constBuffers));
        boost::asio::buffer_copy(boost::asio::buffer(bytes), constBuffers);

        return bytes;
    }
}   // namespace

namespace Flux
{
    HttpProtocolSession::HttpProtocolSession(const std::shared_ptr<HttpRouter>& router)
        : _httpRouter(router)
    {}

    auto HttpProtocolSession::OnBytes(const std::span<const std::byte> bytes) -> bool
    {
        if (!BufferIncomingBytes(bytes))
            return false;

        while (_leftoverBuffer.size() > 0)
        {
            boost::beast::http::request_parser<boost::beast::http::string_body> requestParser;
            boost::beast::error_code errorCode;

            const size_t bytesConsumed = requestParser.put(_leftoverBuffer.data(), errorCode);

            if (errorCode && errorCode != boost::beast::http::error::need_more)
            {
                ErrorLog("Failed to parse HTTP request. Error: {} (code: {})",
                         errorCode.message(),
                         errorCode.value());
                return false;
            }

            if (!requestParser.is_done())
                break;

            _leftoverBuffer.consume(bytesConsumed);

            HttpResponse response;

            const auto rawRequest = requestParser.release();

            if (const HttpRequest httpRequest(rawRequest);
                !_httpRouter->MatchRoute(httpRequest, response))
            {
                response.NotFound().Text("404 Not Found");

                WarnLog("Endpoint not found (404): method={}, path={}",
                        ToString(httpRequest.Method()),
                        httpRequest.Path());
            }

            auto serializedData = SerializeResponse(response);
            if (const auto sendResult = SendToClient(std::move(serializedData)); !sendResult)
            {
                ErrorLog("Failed to send HTTP response: {}", sendResult.error().message());
                return false;
            }
        }

        return true;
    }
}   // namespace Flux
