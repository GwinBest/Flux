#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "IProtocolSession.hpp"

namespace Flux
{
    class HttpSession : public IProtocolSession
    {
    public:
        auto OnBytes(const std::span<const std::byte> bytes) -> bool override;
    };

}   // namespace Flux
