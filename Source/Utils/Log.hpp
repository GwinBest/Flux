#pragma once

#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

namespace Flux::Log
{
    namespace Detail
    {
        [[nodiscard]] inline auto FileName(const std::string_view path) noexcept -> std::string_view
        {
            const auto separator = path.find_last_of("/\\");

            if (separator == std::string_view::npos)
                return path;

            return path.substr(separator + 1U);
        }

        [[nodiscard]] inline auto ToSpdlogSource(const std::source_location location) noexcept
            -> spdlog::source_loc
        {
            return {location.file_name(),
                    static_cast<int>(location.line()),
                    location.function_name()};
        }

        inline void WriteLog(const spdlog::level::level_enum level,
                             const std::string_view message,
                             const std::source_location location)
        {
            spdlog::log(ToSpdlogSource(location),
                        level,
                        "[{}:{}] {}: {}",
                        FileName(location.file_name()),
                        location.line(),
                        location.function_name(),
                        message);
        }
    }   // namespace Detail

    template <typename... Args>
    [[nodiscard]] auto Format(spdlog::format_string_t<Args...> format, Args&&... args)
        -> std::string
    {
        return spdlog::fmt_lib::format(format, std::forward<Args>(args)...);
    }

    inline void DebugLog(const std::string_view message,
                         const std::source_location location = std::source_location::current())
    {
        Detail::WriteLog(spdlog::level::debug, message, location);
    }

    inline void InfoLog(const std::string_view message,
                        const std::source_location location = std::source_location::current())
    {
        Detail::WriteLog(spdlog::level::info, message, location);
    }

    inline void WarnLog(const std::string_view message,
                        const std::source_location location = std::source_location::current())
    {
        Detail::WriteLog(spdlog::level::warn, message, location);
    }

    inline void ErrorLog(const std::string_view message,
                         const std::source_location location = std::source_location::current())
    {
        Detail::WriteLog(spdlog::level::err, message, location);
    }

    inline void ConfigureDefaultLogger()
    {
        spdlog::set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v%$");
    }
}   // namespace Flux::Log
