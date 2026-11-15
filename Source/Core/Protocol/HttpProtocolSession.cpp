#include "HttpProtocolSession.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "Utils/Log.hpp"

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

            auto httpRequest = requestParser.release();

            // TODO: send http request to router
        }

        return true;
    }
}   // namespace Flux
