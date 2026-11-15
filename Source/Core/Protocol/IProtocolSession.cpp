#include "IProtocolSession.hpp"

#include <memory>

#include "Utils/Log.hpp"

namespace Flux
{

    auto IProtocolSession::SetMaxRequestSize(const std::size_t maxRequestSize) noexcept -> void
    {
        _leftoverBuffer.max_size(maxRequestSize);
    }

    auto IProtocolSession::GetMaxRequestSize() const noexcept -> std::size_t
    {
        return _leftoverBuffer.max_size();
    }

    auto IProtocolSession::BufferIncomingBytes(const std::span<const std::byte> bytes) -> bool
    {
        if (bytes.empty())
            return true;

        const auto bufferedSize = _leftoverBuffer.size();
        const auto maxRequestSize = _leftoverBuffer.max_size();
        if (bufferedSize > maxRequestSize || bytes.size() > maxRequestSize - bufferedSize)
        {
            ErrorLog("HTTP request size limit exceeded: buffered={}, incoming={}, limit={}",
                     bufferedSize,
                     bytes.size(),
                     maxRequestSize);
            return false;
        }

        const auto writableSpace = _leftoverBuffer.prepare(bytes.size());
        std::memcpy(writableSpace.data(), bytes.data(), bytes.size());
        _leftoverBuffer.commit(bytes.size());

        return true;
    }
}   // namespace Flux
