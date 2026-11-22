#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <memory>
#include <span>

#include "Protocol/Http/HttpRouter.hpp"
#include "Protocol/IProtocolSession.hpp"

namespace Flux
{
    class HttpProtocolSession final : public IProtocolSession
    {
    public:
        explicit HttpProtocolSession(const std::shared_ptr<HttpRouter>& router);

        auto OnBytes(const std::span<const std::byte> bytes) -> bool override;

    private:
        std::shared_ptr<HttpRouter> _httpRouter;
    };
}   // namespace Flux
