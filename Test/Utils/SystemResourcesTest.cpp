#include "Utils/SystemResources.hpp"
#include "gtest/gtest.h"

#include <cstdint>
#include <limits>
#include <system_error>

#include "Core/ConnectionManager.hpp"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace Flux::Test
{
    TEST(SystemResourcesTest, ReportsNativeResourceLimits)
    {
        const auto result = GetSystemResources();
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
        ASSERT_TRUE(result.has_value()) << result.error().message();
        EXPECT_GT(result->AvailableMemoryBytes, 0U);
#ifdef _WIN32
        EXPECT_EQ(result->FileDescriptorLimit, std::numeric_limits<std::uint64_t>::max());
#else
        rlimit limit {};
        ASSERT_EQ(getrlimit(RLIMIT_NOFILE, &limit), 0);
        const auto expected = limit.rlim_cur == RLIM_INFINITY
                                ? std::numeric_limits<std::uint64_t>::max()
                                : static_cast<std::uint64_t>(limit.rlim_cur);
        EXPECT_EQ(result->FileDescriptorLimit, expected);
#endif
#else
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), std::make_error_code(std::errc::function_not_supported));
#endif
    }

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    TEST(SystemResourcesTest, DetectedResourcesProduceCapacityWithinBothBudgets)
    {
        const auto resources = GetSystemResources();
        ASSERT_TRUE(resources.has_value()) << resources.error().message();
        const ConnectionLimitOptions options;
        const auto capacity = ConnectionManager::CalculateMaxConnections(*resources, options);
        ASSERT_TRUE(capacity.has_value()) << capacity.error().message();
        EXPECT_LE(*capacity, resources->AvailableMemoryBytes / options.MemoryPerConnectionBytes);
        if (resources->FileDescriptorLimit <= options.ReservedFileDescriptors)
            EXPECT_EQ(*capacity, 0U);
        else
            EXPECT_LE(*capacity, resources->FileDescriptorLimit - options.ReservedFileDescriptors);

        const ConnectionManager manager;
        EXPECT_TRUE(manager.CloseAllConnections().has_value());
    }
#endif
}   // namespace Flux::Test
