#pragma once

#include <cstdint>
#include <expected>
#include <system_error>

namespace Flux
{
    struct SystemResources final
    {
        std::uint64_t FileDescriptorLimit = 0;
        std::uint64_t AvailableMemoryBytes = 0;
    };

    [[nodiscard]] auto GetSystemResources() -> std::expected<SystemResources, std::error_code>;
}   // namespace Flux
