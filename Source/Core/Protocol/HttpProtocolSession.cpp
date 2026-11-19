#include "HttpProtocolSession.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "Utils/Log.hpp"

namespace
{
    auto SerializeResponse(const Flux::HttpResponse& response) -> std::vector<std::byte>
    {
        boost::beast::http::response_serializer<Flux::HttpResponse::body_type> serializer {
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

            if (const HttpRequest httpRequest = requestParser.release();
                !_httpRouter.MatchRoute(httpRequest, response))
            {
                response.result(boost::beast::http::status::not_found);
                response.set(boost::beast::http::field::content_type, "text/plain");
                response.body() = "404 Not Found";
                response.prepare_payload();

                WarnLog("Endpoint not found (404): method={}, path={}",
                        to_string(httpRequest.method()),
                        httpRequest.target());
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
