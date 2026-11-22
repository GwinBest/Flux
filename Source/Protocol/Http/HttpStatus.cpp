#include "HttpStatus.hpp"

#include <utility>

namespace Flux
{
    auto ToInteger(const HttpStatus status) -> std::uint16_t
    {
        return std::to_underlying(status);
    }
}   // namespace Flux
