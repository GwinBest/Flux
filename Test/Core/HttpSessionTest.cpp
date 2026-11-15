#include "gtest/gtest.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <string_view>

#include "Core/Protocol/HttpProtocolSession.hpp"

namespace Flux::Test
{
    TEST(HttpSessionTest, RequestLimitAccessorsPreserveZeroOneMinAndMaximumValues)
    {
        HttpProtocolSession session;
        constexpr auto maximum = std::numeric_limits<std::size_t>::max();
        constexpr auto minimum = std::numeric_limits<std::size_t>::min();
        for (const auto limit : {std::size_t {0}, std::size_t {1}, minimum, maximum})
        {
            SCOPED_TRACE(limit);
            session.SetMaxRequestSize(limit);
            EXPECT_EQ(session.GetMaxRequestSize(), limit);
        }
    }

    TEST(HttpSessionTest, SupportsDestructionThroughProtocolInterface)
    {
        const std::unique_ptr<IProtocolSession> session = std::make_unique<HttpProtocolSession>();
        session->SetMaxRequestSize(1);
        EXPECT_EQ(session->GetMaxRequestSize(), 1U);
        EXPECT_TRUE(session->OnBytes({}));
    }

    TEST(HttpSessionTest, RejectsInputThatWouldExceedConfiguredLimit)
    {
        HttpProtocolSession session;
        session.SetMaxRequestSize(4);
        constexpr std::string_view bytes = "GET / HTTP/1.1\r\n";

        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(bytes.data(), bytes.size()))));
    }

    TEST(HttpSessionTest, AllowsEmptyInputWithZeroLimit)
    {
        HttpProtocolSession session;
        session.SetMaxRequestSize(0);

        EXPECT_TRUE(session.OnBytes({}));
    }

    TEST(HttpSessionTest, RejectsBytesThatExceedLimitAfterPartialRequest)
    {
        HttpProtocolSession session;
        constexpr std::string_view
            request = "POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 4\r\n\r\nbody";
        session.SetMaxRequestSize(request.size() - 1);
        const auto requestBytes = std::as_bytes(std::span(request.data(), request.size()));

        ASSERT_TRUE(session.OnBytes(requestBytes.first(requestBytes.size() - 1)));
        EXPECT_FALSE(session.OnBytes(requestBytes.last(1)));
    }

    TEST(HttpSessionTest, AcceptsRequestDeliveredOneByteAtATime)
    {
        HttpProtocolSession session;
        constexpr std::string_view request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
        session.SetMaxRequestSize(request.size());

        for (const auto character : request)
        {
            const auto byte = std::as_bytes(std::span(&character, 1));
            EXPECT_TRUE(session.OnBytes(byte));
        }
    }

    TEST(HttpSessionTest, AcceptsRequestWithBodySplitAcrossReadsAtExactLimit)
    {
        HttpProtocolSession session;
        constexpr std::string_view
            request = "POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: 4\r\n\r\nbody";
        session.SetMaxRequestSize(request.size());
        const auto requestBytes = std::as_bytes(std::span(request.data(), request.size()));
        constexpr auto bodyOffset = request.size() - 4;

        ASSERT_TRUE(session.OnBytes(requestBytes.first(bodyOffset + 2)));
        EXPECT_TRUE(session.OnBytes(requestBytes.subspan(bodyOffset + 2)));
    }

    TEST(HttpSessionTest, AcceptsChunkedBodySplitAcrossReadsAtExactLimit)
    {
        HttpProtocolSession session;
        constexpr std::string_view
            request = "POST /upload HTTP/1.1\r\nHost: example.com\r\n"
                      "Transfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n0\r\n\r\n";
        session.SetMaxRequestSize(request.size());
        const auto requestBytes = std::as_bytes(std::span(request.data(), request.size()));
        constexpr auto splitOffset = request.size() - 3;

        ASSERT_TRUE(session.OnBytes(requestBytes.first(splitOffset)));
        EXPECT_TRUE(session.OnBytes(requestBytes.subspan(splitOffset)));
    }

    TEST(HttpSessionTest, AcceptsPipelinedRequestsSplitAcrossReads)
    {
        HttpProtocolSession session;
        constexpr std::string_view
            firstRequest = "GET /first HTTP/1.1\r\nHost: example.com\r\n\r\n";
        constexpr std::string_view
            secondRequest = "GET /second HTTP/1.1\r\nHost: example.com\r\n\r\n";
        constexpr auto maxRequestSize = firstRequest.size() + secondRequest.size();
        session.SetMaxRequestSize(maxRequestSize);

        const auto firstBytes = std::as_bytes(std::span(firstRequest.data(), firstRequest.size()));
        const auto secondBytes = std::as_bytes(
            std::span(secondRequest.data(), secondRequest.size()));
        constexpr std::size_t secondRequestPrefixSize = 9;

        ASSERT_TRUE(session.OnBytes(firstBytes));
        ASSERT_TRUE(session.OnBytes(secondBytes.first(secondRequestPrefixSize)));
        EXPECT_TRUE(session.OnBytes(secondBytes.subspan(secondRequestPrefixSize)));
    }

    TEST(HttpSessionTest, RejectsMalformedRequest)
    {
        HttpProtocolSession session;
        constexpr std::string_view request = "G?T / HTTP/1.1\r\nHost: example.com\r\n\r\n";

        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    TEST(HttpSessionTest, ZeroLimitRejectsNonEmptyInput)
    {
        HttpProtocolSession session;
        session.SetMaxRequestSize(0);
        constexpr std::string_view request = "G";

        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    TEST(HttpSessionTest, RejectsPipelinedRequestsExceedingBufferLimit)
    {
        HttpProtocolSession session;
        constexpr std::string_view request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
        constexpr std::string_view
            pipelinedRequests = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n"
                                "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
        session.SetMaxRequestSize(request.size());

        EXPECT_GT(pipelinedRequests.size(), session.GetMaxRequestSize());
        EXPECT_FALSE(session.OnBytes(
            std::as_bytes(std::span(pipelinedRequests.data(), pipelinedRequests.size()))));
    }

    TEST(HttpSessionTest, ProcessesPipelinedRequestsWithinBufferLimit)
    {
        HttpProtocolSession session;
        constexpr std::string_view
            pipelinedRequests = "GET /first HTTP/1.1\r\nHost: example.com\r\n\r\n"
                                "GET /second HTTP/1.1\r\nHost: example.com\r\n\r\n";
        session.SetMaxRequestSize(pipelinedRequests.size());

        EXPECT_TRUE(session.OnBytes(
            std::as_bytes(std::span(pipelinedRequests.data(), pipelinedRequests.size()))));
    }
}   // namespace Flux::Test
