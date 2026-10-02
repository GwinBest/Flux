#include "Utils/Log.hpp"
#include "gtest/gtest.h"

#include <memory>
#include <mutex>
#include <source_location>
#include <spdlog/sinks/base_sink.h>
#include <string>
#include <string_view>
#include <vector>

namespace Flux::Test
{
    namespace
    {
        struct LogEntry final
        {
            spdlog::level::level_enum Level;
            std::string Text;
            std::string File;
            int Line;
            std::string Function;
        };

        class RecordingSink final : public spdlog::sinks::base_sink<std::mutex>
        {
        public:
            std::vector<LogEntry> Entries;

        protected:
            void sink_it_(const spdlog::details::log_msg& message) override
            {
                Entries.push_back({
                    .Level = message.level,
                    .Text = std::string(message.payload.data(), message.payload.size()),
                    .File = message.source.filename,
                    .Line = message.source.line,
                    .Function = message.source.funcname,
                });
            }

            void flush_() override {}
        };

        class LogTest : public testing::Test
        {
        protected:
            void SetUp() override
            {
                _previousLogger = spdlog::default_logger();
                _sink = std::make_shared<RecordingSink>();
                auto logger = std::make_shared<spdlog::logger>("test", _sink);
                logger->set_level(spdlog::level::trace);
                spdlog::set_default_logger(std::move(logger));
            }

            void TearDown() override { spdlog::set_default_logger(_previousLogger); }

            std::shared_ptr<RecordingSink> _sink;

        private:
            std::shared_ptr<spdlog::logger> _previousLogger;
        };
    }   // namespace

    TEST(LogMessageTest, PreservesTextAndExplicitSourceLocation)
    {
        const auto location = std::source_location::current();
        const LogMessage literal("literal", location);
        const std::string owned = "owned";
        const LogMessage string(owned, location);
        constexpr std::string_view view("prefix suffix", 6);
        const LogMessage substring(view, location);
        const auto* pointer = "pointer";
        const LogMessage pointed(pointer, location);

        EXPECT_EQ(literal.Text, "literal");
        EXPECT_EQ(string.Text, owned);
        EXPECT_EQ(substring.Text, "prefix");
        EXPECT_EQ(pointed.Text, "pointer");
        EXPECT_EQ(string.Text.data(), owned.data());
        EXPECT_EQ(literal.Location.line(), location.line());
        EXPECT_STREQ(literal.Location.file_name(), location.file_name());
        EXPECT_STREQ(literal.Location.function_name(), location.function_name());

        const auto source = ToSpdlogSource(location);
        EXPECT_STREQ(source.filename, location.file_name());
        EXPECT_EQ(source.line, static_cast<int>(location.line()));
        EXPECT_STREQ(source.funcname, location.function_name());
    }

    TEST_F(LogTest, WritesEverySeverityWithAndWithoutFormatting)
    {
        DebugLog("debug {}", 1);
        DebugLog("debug");
        InfoLog("info {}", 2);
        InfoLog("info");
        WarnLog("warn {}", 3);
        WarnLog("warn");
        ErrorLog("error {}", 4);
        ErrorLog("error");

        ASSERT_EQ(_sink->Entries.size(), 8U);
        const std::vector<std::string>
            texts {"debug 1", "debug", "info 2", "info", "warn 3", "warn", "error 4", "error"};
        const std::vector levels {
            spdlog::level::debug,
            spdlog::level::info,
            spdlog::level::warn,
            spdlog::level::err,
        };
        for (std::size_t index = 0; index < texts.size(); ++index)
        {
            EXPECT_EQ(_sink->Entries.at(index).Text, texts.at(index));
            EXPECT_EQ(_sink->Entries.at(index).Level, levels.at(index / 2));
        }
    }

    TEST_F(LogTest, PreservesBracesWithoutArgumentsAndFormatsRuntimeText)
    {
        InfoLog("unformatted {braces}");
        const std::string pattern = "{} = {:.2f}";
        constexpr double formattedValue = 1.25;
        InfoLog(pattern, std::string("value"), formattedValue);
        InfoLog(std::string_view {});

        ASSERT_EQ(_sink->Entries.size(), 3U);
        EXPECT_EQ(_sink->Entries.at(0).Text, "unformatted {braces}");
        EXPECT_EQ(_sink->Entries.at(1).Text, "value = 1.25");
        EXPECT_TRUE(_sink->Entries.at(2).Text.empty());
    }

    TEST_F(LogTest, PassesCallSiteToLogger)
    {
        const auto location = std::source_location::current();
        InfoLog(LogMessage("message", location));

        ASSERT_EQ(_sink->Entries.size(), 1U);
        EXPECT_EQ(_sink->Entries.at(0).File, location.file_name());
        EXPECT_EQ(_sink->Entries.at(0).Line, static_cast<int>(location.line()));
        EXPECT_EQ(_sink->Entries.at(0).Function, location.function_name());
    }

    TEST_F(LogTest, PropagatesInvalidFormatWithoutWritingPartialMessage)
    {
        EXPECT_THROW(InfoLog("{", 1), spdlog::fmt_lib::format_error);
        EXPECT_TRUE(_sink->Entries.empty());
    }

    TEST_F(LogTest, RespectsConfiguredSeverityThreshold)
    {
        spdlog::default_logger()->set_level(spdlog::level::warn);
        DebugLog("hidden {}", 1);
        InfoLog("hidden info");
        WarnLog("visible {}", 2);
        ErrorLog("visible error");

        ASSERT_EQ(_sink->Entries.size(), 2U);
        EXPECT_EQ(_sink->Entries.at(0).Level, spdlog::level::warn);
        EXPECT_EQ(_sink->Entries.at(0).Text, "visible 2");
        EXPECT_EQ(_sink->Entries.at(1).Level, spdlog::level::err);
        EXPECT_EQ(_sink->Entries.at(1).Text, "visible error");
    }
}   // namespace Flux::Test
