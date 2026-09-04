#pragma once

#include <source_location>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace Flux
{
    struct LogMessage final
    {
        template<typename Message>
        LogMessage(
            const Message& messageText,
            const std::source_location sourceLocation = std::source_location::current()) noexcept
            : Text(messageText)
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

    template<typename... Args>
    void WriteLog(const spdlog::level::level_enum level, const LogMessage message, Args&&... args)
    {
        std::string text(message.Text);

        if constexpr (sizeof...(Args) > 0)
            text = spdlog::fmt_lib::format(SPDLOG_FMT_RUNTIME(message.Text),
                                           std::forward<Args>(args)...);

        spdlog::log(ToSpdlogSource(message.Location), level, "{}", text);
    }

    template<typename... Args>
    void DebugLog(const std::type_identity_t<LogMessage> message, Args&&... args)
    {
        WriteLog(spdlog::level::debug, message, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void InfoLog(const std::type_identity_t<LogMessage> message, Args&&... args)
    {
        WriteLog(spdlog::level::info, message, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void WarnLog(const std::type_identity_t<LogMessage> message, Args&&... args)
    {
        WriteLog(spdlog::level::warn, message, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void ErrorLog(const std::type_identity_t<LogMessage> message, Args&&... args)
    {
        WriteLog(spdlog::level::err, message, std::forward<Args>(args)...);
    }
}   // namespace Flux
