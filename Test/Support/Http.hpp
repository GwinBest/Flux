#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Flux::Test
{
    inline auto MakeHttpPostRequestWithSize(const std::size_t totalSize) -> std::string
    {
        constexpr std::string_view
            headerPrefix = "POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: ";
        constexpr std::string_view headerSuffix = "\r\n\r\n";

        auto bodySize = totalSize;
        for (;;)
        {
            const auto headerSize = headerPrefix.size() + std::to_string(bodySize).size()
                                  + headerSuffix.size();
            if (headerSize > totalSize)
                throw std::invalid_argument("Request size is too small for an HTTP POST");

            const auto nextBodySize = totalSize - headerSize;
            if (nextBodySize == bodySize)
            {
                return std::string(headerPrefix) + std::to_string(bodySize)
                     + std::string(headerSuffix) + std::string(bodySize, 'x');
            }

            bodySize = nextBodySize;
        }
    }
}   // namespace Flux::Test
