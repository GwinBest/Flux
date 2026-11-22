#include "SystemResources.hpp"

#include <algorithm>
#include <cerrno>
#include <expected>
#include <limits>
#include <system_error>

#include "Utils/Error.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#ifdef __linux__
#include <fstream>
#include <sstream>
#include <string>
#include <sys/sysinfo.h>
#else
#include <mach/mach.h>
#endif
#endif

namespace Flux
{
    namespace
    {
#if defined(__linux__) || defined(__APPLE__)
        auto GetFileDescriptorLimit() -> std::expected<std::uint64_t, std::error_code>
        {
            rlimit limit {};
            if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
            {
                return std::unexpected(std::error_code(errno, std::generic_category()));
            }

            return limit.rlim_cur == RLIM_INFINITY ? std::numeric_limits<std::uint64_t>::max()
                                                   : static_cast<std::uint64_t>(limit.rlim_cur);
        }

        auto MemoryUnitsToBytes(const std::uint64_t units, const std::uint64_t bytesPerUnit)
            -> std::expected<std::uint64_t, std::error_code>
        {
            if (bytesPerUnit == 0)
            {
                return std::unexpected(MakeErrorCode(Error::InvalidArgument));
            }
            if (units > std::numeric_limits<std::uint64_t>::max() / bytesPerUnit)
            {
                return std::unexpected(MakeErrorCode(Error::ResultOutOfRange));
            }

            return units * bytesPerUnit;
        }
#endif

#ifdef __linux__
        auto GetAvailableMemoryBytes() -> std::expected<std::uint64_t, std::error_code>
        {
            std::ifstream memoryInfo("/proc/meminfo");
            if (memoryInfo.is_open())
            {
                std::string line;
                while (std::getline(memoryInfo, line))
                {
                    std::istringstream entry(line);
                    std::string name;
                    if (!(entry >> name) || name != "MemAvailable:")
                        continue;

                    std::uint64_t availableKiB = 0;
                    std::string unit;
                    if (!(entry >> availableKiB >> unit) || unit != "kB")
                    {
                        return std::unexpected(MakeErrorCode(Error::IoError));
                    }

                    constexpr std::uint64_t bytesPerKiB = 1024;
                    return MemoryUnitsToBytes(availableKiB, bytesPerKiB);
                }
            }

            struct sysinfo memory {};
            if (sysinfo(&memory) != 0)
            {
                return std::unexpected(std::error_code(errno, std::generic_category()));
            }

            return MemoryUnitsToBytes(static_cast<std::uint64_t>(memory.freeram),
                                      static_cast<std::uint64_t>(memory.mem_unit));
        }
#elifdef __APPLE__
        auto GetAvailableMemoryBytes() -> std::expected<std::uint64_t, std::error_code>
        {
            vm_size_t pageSize = 0;
            vm_statistics64_data_t memory {};
            mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
            const auto host = mach_host_self();
            const auto pageResult = host_page_size(host, &pageSize);

            const auto statisticsBuffer = reinterpret_cast<host_info64_t>(&memory);
            const auto statisticsResult = host_statistics64(host,
                                                            HOST_VM_INFO64,
                                                            statisticsBuffer,
                                                            &count);
            const auto releaseResult = mach_port_deallocate(mach_task_self(), host);

            if (pageResult != KERN_SUCCESS || statisticsResult != KERN_SUCCESS
                || releaseResult != KERN_SUCCESS)
            {
                return std::unexpected(MakeErrorCode(Error::StateNotRecoverable));
            }

            const auto availablePages = static_cast<std::uint64_t>(memory.free_count)
                                      + static_cast<std::uint64_t>(memory.inactive_count);
            return MemoryUnitsToBytes(availablePages, static_cast<std::uint64_t>(pageSize));
        }
#endif
    }   // namespace

    auto GetSystemResources() -> std::expected<SystemResources, std::error_code>
    {
#ifdef _WIN32
        MEMORYSTATUSEX memory {};
        memory.dwLength = static_cast<DWORD>(sizeof(memory));
        if (GlobalMemoryStatusEx(&memory) == 0)
        {
            return std::unexpected(
                std::error_code(static_cast<int>(GetLastError()), std::system_category()));
        }

        const auto availableMemory = std::min(
            {memory.ullAvailPhys, memory.ullAvailVirtual, memory.ullAvailPageFile});

        return SystemResources {
            .FileDescriptorLimit = std::numeric_limits<std::uint64_t>::max(),
            .AvailableMemoryBytes = availableMemory,
        };
#elif defined(__linux__) || defined(__APPLE__)
        auto fdLimit = GetFileDescriptorLimit();
        if (!fdLimit)
        {
            return std::unexpected(fdLimit.error());
        }

        auto availMem = GetAvailableMemoryBytes();
        if (!availMem)
        {
            return std::unexpected(availMem.error());
        }

        return SystemResources {.FileDescriptorLimit = *fdLimit, .AvailableMemoryBytes = *availMem};
#else
        return std::unexpected(MakeErrorCode(Error::FunctionNotSupported));
#endif
    }
}   // namespace Flux
