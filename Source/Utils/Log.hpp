#pragma once

#include <source_location>
#include <spdlog/spdlog.h>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace Flux
{
    struct LogMessage final
    {
        template<typename Message>
        explicit(false) LogMessage(
            const Message& messageText,
            const std::source_location sourceLocation = std::source_location::current()) noexcept
            : Text([&messageText] -> std::string_view {
                if constexpr (std::is_array_v<Message>)
                    return std::string_view(static_cast<const char*>(messageText));
                else
                    return std::string_view(messageText);
            }())
            , Location(sourceLocation)
        {}

        std::string_view Text;
        std::source_location Location;
    };

    [[nodiscard]] inline auto ToSpdlogSource(const std::source_location location) noexcept
        -> spdlog::source_loc
    {
        return {location.file_name(), static_cast<int>(location.line()), location.function_name()};
    }

    inline auto GetLogMutex() noexcept -> std::mutex&
    {
        static std::mutex mutex;
        return mutex;
    }

    template<typename... Args>
    void WriteLog(const spdlog::level::level_enum level, const LogMessage message, Args&&... args)
    {
        std::string text(message.Text);

        if constexpr (sizeof...(Args) > 0)
            text = spdlog::fmt_lib::format(SPDLOG_FMT_RUNTIME(message.Text),
                                           std::forward<Args>(args)...);

        const std::scoped_lock lock(GetLogMutex());
        spdlog::log(ToSpdlogSource(message.Location), level, "{}", text);
    }

    template<typename... Args>
    void DebugLog(const LogMessage message, Args&&... args)
    {
        WriteLog(spdlog::level::debug, message, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void InfoLog(const LogMessage message, Args&&... args)
    {
        WriteLog(spdlog::level::info, message, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void WarnLog(const LogMessage message, Args&&... args)
    {
        WriteLog(spdlog::level::warn, message, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void ErrorLog(const LogMessage message, Args&&... args)
    {
        WriteLog(spdlog::level::err, message, std::forward<Args>(args)...);
    }
}   // namespace Flux
