#include "gtest/gtest.h"

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Protocol/Http/HttpField.hpp"
#include "Protocol/Http/HttpProtocolSession.hpp"
#include "Protocol/Http/HttpRequest.hpp"
#include "Protocol/Http/HttpResponse.hpp"
#include "Protocol/Http/HttpRouter.hpp"

namespace Flux::Test
{
    namespace
    {
        constexpr std::size_t DefaultRequestSize = 1024;
        constexpr std::size_t LargeRequestSize = 4096;
        constexpr int HeaderCount = 50;

        auto MakeDefaultRouter() -> std::shared_ptr<HttpRouter>
        {
            return std::make_shared<HttpRouter>();
        }
    }   // namespace

    TEST(HttpSessionTest, RequestLimitAccessorsPreserveZeroOneMinAndMaximumValues)
    {
        HttpProtocolSession session(MakeDefaultRouter());
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
        const std::unique_ptr<IProtocolSession> session = std::make_unique<HttpProtocolSession>(
            MakeDefaultRouter());
        session->SetMaxRequestSize(1);
        EXPECT_EQ(session->GetMaxRequestSize(), 1U);
        EXPECT_TRUE(session->OnBytes({}));
    }

    TEST(HttpSessionTest, RejectsInputThatWouldExceedConfiguredLimit)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(4);
        constexpr std::string_view bytes = "GET / HTTP/1.1\r\n";

        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(bytes.data(), bytes.size()))));
    }

    TEST(HttpSessionTest, AllowsEmptyInputWithZeroLimit)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(0);

        EXPECT_TRUE(session.OnBytes({}));
    }

    TEST(HttpSessionTest, RejectsBytesThatExceedLimitAfterPartialRequest)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        constexpr std::string_view
            request = "POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 4\r\n\r\nbody";
        session.SetMaxRequestSize(request.size() - 1);
        const auto requestBytes = std::as_bytes(std::span(request.data(), request.size()));

        ASSERT_TRUE(session.OnBytes(requestBytes.first(requestBytes.size() - 1)));
        EXPECT_FALSE(session.OnBytes(requestBytes.last(1)));
    }

    TEST(HttpSessionTest, AcceptsRequestDeliveredOneByteAtATime)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        constexpr std::string_view request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
        session.SetMaxRequestSize(request.size());
        session.SetSendCallback(
            [](std::vector<std::byte>&&) -> Result {   // NOLINT(*-rvalue-reference-param-not-moved)
                return Result {};
            });

        for (const auto character : request)
        {
            const auto byte = std::as_bytes(std::span(&character, 1));
            EXPECT_TRUE(session.OnBytes(byte));
        }
    }

    TEST(HttpSessionTest, AcceptsRequestWithBodySplitAcrossReadsAtExactLimit)
    {
        HttpProtocolSession session(MakeDefaultRouter());
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
        HttpProtocolSession session(MakeDefaultRouter());
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
        HttpProtocolSession session(MakeDefaultRouter());
        constexpr std::string_view
            firstRequest = "GET /first HTTP/1.1\r\nHost: example.com\r\n\r\n";
        constexpr std::string_view
            secondRequest = "GET /second HTTP/1.1\r\nHost: example.com\r\n\r\n";
        constexpr auto maxRequestSize = firstRequest.size() + secondRequest.size();
        session.SetMaxRequestSize(maxRequestSize);
        session.SetSendCallback(
            [](std::vector<std::byte>&&) -> Result {   // NOLINT(*-rvalue-reference-param-not-moved)
                return Result {};
            });

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
        HttpProtocolSession session(MakeDefaultRouter());
        constexpr std::string_view request = "G?T / HTTP/1.1\r\nHost: example.com\r\n\r\n";

        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    TEST(HttpSessionTest, ZeroLimitRejectsNonEmptyInput)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(0);
        constexpr std::string_view request = "G";

        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    TEST(HttpSessionTest, RejectsPipelinedRequestsExceedingBufferLimit)
    {
        HttpProtocolSession session(MakeDefaultRouter());
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
        HttpProtocolSession session(MakeDefaultRouter());
        constexpr std::string_view
            pipelinedRequests = "GET /first HTTP/1.1\r\nHost: example.com\r\n\r\n"
                                "GET /second HTTP/1.1\r\nHost: example.com\r\n\r\n";
        session.SetMaxRequestSize(pipelinedRequests.size());
        session.SetSendCallback(
            [](std::vector<std::byte>&&) -> Result {   // NOLINT(*-rvalue-reference-param-not-moved)
                return Result {};
            });

        EXPECT_TRUE(session.OnBytes(
            std::as_bytes(std::span(pipelinedRequests.data(), pipelinedRequests.size()))));
    }

    // === Constructor with null router ===

    TEST(HttpSessionTest, ConstructorWithNullRouter)
    {
        const std::shared_ptr<HttpRouter> nullRouter;
        HttpProtocolSession session(nullRouter);
        session.SetMaxRequestSize(DefaultRequestSize);

        // Empty input should succeed even with null router
        EXPECT_TRUE(session.OnBytes({}));
    }

    TEST(HttpSessionTest, ConstructorWithNullRouterRejectsMalformedRequest)
    {
        const std::shared_ptr<HttpRouter> nullRouter;
        HttpProtocolSession session(nullRouter);
        constexpr std::string_view request = "G?T / HTTP/1.1\r\nHost: example.com\r\n\r\n";

        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    // === Send callback tests ===

    TEST(HttpSessionTest, SendCallbackNotSetReturnsTrueForEmptyInput)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        // No send callback set — empty input should still succeed
        EXPECT_TRUE(session.OnBytes({}));
    }

    TEST(HttpSessionTest, SendCallbackSetAndUsed)
    {
        const auto router = std::make_shared<HttpRouter>();
        bool handlerCalled = false;
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/test"},
                               [&handlerCalled](const HttpRequest&, HttpResponse& resp) -> void {
                                   handlerCalled = true;
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "GET /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_TRUE(handlerCalled);
        EXPECT_FALSE(receivedData.empty());
    }

    TEST(HttpSessionTest, SendCallbackReturnsError)
    {
        const auto router = std::make_shared<HttpRouter>();
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/test"},
                               [](const HttpRequest&, HttpResponse& resp) -> void {
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        session.SetSendCallback(
            [](std::vector<std::byte>&&) -> Result {   // NOLINT(*-rvalue-reference-param-not-moved)
                return std::unexpected(MakeErrorCode(Error::ConnectionClosed));
            });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "GET /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    // === HTTP Framing tests ===

    TEST(HttpSessionTest, FramingIncompleteRequestHeader)
    {
        const auto router = std::make_shared<HttpRouter>();
        bool handlerCalled = false;
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/test"},
                               [&handlerCalled](const HttpRequest&, HttpResponse& resp) -> void {
                                   handlerCalled = true;
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        // Send only part of the request line — should not trigger handler
        constexpr std::string_view partial = "GET /test HTTP/1.1\r\nHos";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(partial.data(), partial.size()))));
        EXPECT_FALSE(handlerCalled);
    }

    TEST(HttpSessionTest, FramingCompleteRequestAfterPartial)
    {
        const auto router = std::make_shared<HttpRouter>();
        bool handlerCalled = false;
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/test"},
                               [&handlerCalled](const HttpRequest&, HttpResponse& resp) -> void {
                                   handlerCalled = true;
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view part1 = "GET /test HTTP/1.1\r\n";
        constexpr std::string_view part2 = "Host: example.com\r\n\r\n";

        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(part1.data(), part1.size()))));
        EXPECT_FALSE(handlerCalled);

        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(part2.data(), part2.size()))));
        EXPECT_TRUE(handlerCalled);
    }

    TEST(HttpSessionTest, FramingPostWithContentLength)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(DefaultRequestSize);

        // POST with Content-Length: the parser::put() with flat_buffer::data()
        // buffers the data but may not complete parsing the body in one call.
        // Verify that OnBytes accepts the data without error.
        constexpr std::string_view
            request = "POST /data HTTP/1.1\r\nHost: example.com\r\nContent-Length: 5\r\n\r\nhello";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    TEST(HttpSessionTest, FramingPostBodySplitAcrossReads)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(DefaultRequestSize);

        // POST body split across reads: headers first, then body.
        // The parser buffers data across calls — verify no error.
        constexpr std::string_view
            headers = "POST /data HTTP/1.1\r\nHost: example.com\r\nContent-Length: 5\r\n\r\n";
        constexpr std::string_view body = "hello";

        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(headers.data(), headers.size()))));
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(body.data(), body.size()))));
    }

    TEST(HttpSessionTest, FramingChunkedTransferEncoding)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(DefaultRequestSize);

        // Chunked transfer encoding: verify the session accepts chunked data
        constexpr std::string_view request = "POST /chunked HTTP/1.1\r\nHost: example.com\r\n"
                                             "Transfer-Encoding: chunked\r\n\r\n"
                                             "5\r\nhello\r\n0\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    TEST(HttpSessionTest, FramingChunkedSplitAcrossReads)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(DefaultRequestSize);

        // Chunked data split across reads: headers first, then chunks
        constexpr std::string_view part1 = "POST /chunked HTTP/1.1\r\nHost: example.com\r\n"
                                           "Transfer-Encoding: chunked\r\n\r\n";
        constexpr std::string_view part2 = "5\r\nhello\r\n0\r\n\r\n";

        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(part1.data(), part1.size()))));
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(part2.data(), part2.size()))));
    }

    TEST(HttpSessionTest, FramingMultipleChunks)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(DefaultRequestSize);

        // Multiple chunks in one request: verify the session accepts the data
        constexpr std::string_view request = "POST /multi HTTP/1.1\r\nHost: example.com\r\n"
                                             "Transfer-Encoding: chunked\r\n\r\n"
                                             "5\r\nhello\r\n5\r\nworld\r\n0\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    TEST(HttpSessionTest, FramingPipelinedRequests)
    {
        const auto router = std::make_shared<HttpRouter>();
        int callCount = 0;
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/first"},
                               [&callCount](const HttpRequest&, HttpResponse& resp) -> void {
                                   ++callCount;
                                   resp.Ok();
                                   resp.Body("First");
                               });
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/second"},
                               [&callCount](const HttpRequest&, HttpResponse& resp) -> void {
                                   ++callCount;
                                   resp.Ok();
                                   resp.Body("Second");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view pipelined = "GET /first HTTP/1.1\r\nHost: example.com\r\n\r\n"
                                               "GET /second HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(pipelined.data(), pipelined.size()))));
        EXPECT_EQ(callCount, 2);
    }

    TEST(HttpSessionTest, FramingRequestWithExtraHeaders)
    {
        const auto router = std::make_shared<HttpRouter>();
        std::string capturedHeader;
        (void)router->AddRoute(
            {.Method = HttpMethod::Get, .Path = "/headers"},
            [&capturedHeader](const HttpRequest& req, HttpResponse& resp) -> void {
                if (req.HasHeader(HttpField::UserAgent))
                {
                    capturedHeader = std::string(req.Header(HttpField::UserAgent));
                }
                resp.Ok();
                resp.Body("OK");
            });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "GET /headers HTTP/1.1\r\n"
                                             "Host: example.com\r\n"
                                             "User-Agent: TestClient/1.0\r\n"
                                             "Accept: */*\r\n"
                                             "\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_EQ(capturedHeader, "TestClient/1.0");
    }

    TEST(HttpSessionTest, Framing404NotFound)
    {
        const auto router = std::make_shared<HttpRouter>();
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/exists"},
                               [](const HttpRequest&, HttpResponse& resp) -> void {
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "GET /notfound HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        // Response should contain 404
        const std::string responseStr(receivedData.begin(), receivedData.end());
        EXPECT_TRUE(responseStr.contains("404"));
    }

    // === Boundary value tests ===

    TEST(HttpSessionTest, BoundaryMaxRequestSizeZero)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(0);

        EXPECT_TRUE(session.OnBytes({}));
    }

    TEST(HttpSessionTest, BoundaryMaxRequestSizeOne)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(1);

        // Single byte should be accepted but parsing will fail (incomplete)
        constexpr std::array<std::byte, 1> singleByte = {std::byte {'G'}};
        EXPECT_TRUE(session.OnBytes(singleByte));
    }

    TEST(HttpSessionTest, BoundaryMaxRequestSizeMax)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(std::numeric_limits<std::size_t>::max());

        EXPECT_TRUE(session.OnBytes({}));
    }

    TEST(HttpSessionTest, BoundaryMaxRequestSizeMin)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(std::numeric_limits<std::size_t>::min());

        EXPECT_TRUE(session.OnBytes({}));
    }

    TEST(HttpSessionTest, BoundaryEmptySpan)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::span<const std::byte> emptySpan {};
        EXPECT_TRUE(session.OnBytes(emptySpan));
    }

    TEST(HttpSessionTest, BoundarySingleByteSpan)
    {
        const auto router = std::make_shared<HttpRouter>();
        HttpProtocolSession session(router);
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::array<std::byte, 1> singleByte = {std::byte {'G'}};
        EXPECT_TRUE(session.OnBytes(singleByte));
    }

    // === Malformed request tests ===

    TEST(HttpSessionTest, MalformedInvalidMethod)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "INVALID / HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    TEST(HttpSessionTest, MalformedMissingVersion)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "GET /\r\nHost: example.com\r\n\r\n";
        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    TEST(HttpSessionTest, MalformedMissingPath)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "GET HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    TEST(HttpSessionTest, MalformedIncompleteHeaders)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(DefaultRequestSize);

        // Headers without final CRLF — parser returns need_more, not an error
        // OnBytes returns true because the data is buffered for more input
        constexpr std::string_view request = "GET / HTTP/1.1\r\nHost: example.com\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    TEST(HttpSessionTest, MalformedInvalidContentLength)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view
            request = "POST / HTTP/1.1\r\nHost: example.com\r\nContent-Length: abc\r\n\r\n";
        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    TEST(HttpSessionTest, MalformedContentLengthMismatch)
    {
        const auto router = std::make_shared<HttpRouter>();
        (void)router->AddRoute({.Method = HttpMethod::Post, .Path = "/data"},
                               [](const HttpRequest&, HttpResponse& resp) -> void {
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        // Content-Length says 10 but body is only 5 bytes
        constexpr std::string_view
            request = "POST /data HTTP/1.1\r\nHost: example.com\r\nContent-Length: 10\r\n\r\nhello";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        // The parser will wait for more data, so it should not have processed yet
    }

    // === HTTP version tests ===

    TEST(HttpSessionTest, HttpVersion10)
    {
        const auto router = std::make_shared<HttpRouter>();
        bool handlerCalled = false;
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/test"},
                               [&handlerCalled](const HttpRequest&, HttpResponse& resp) -> void {
                                   handlerCalled = true;
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "GET /test HTTP/1.0\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_TRUE(handlerCalled);
    }

    TEST(HttpSessionTest, HttpVersion11)
    {
        const auto router = std::make_shared<HttpRouter>();
        bool handlerCalled = false;
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/test"},
                               [&handlerCalled](const HttpRequest&, HttpResponse& resp) -> void {
                                   handlerCalled = true;
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "GET /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_TRUE(handlerCalled);
    }

    // === Response serialization tests ===

    TEST(HttpSessionTest, ResponseContainsStatusLine)
    {
        const auto router = std::make_shared<HttpRouter>();
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/test"},
                               [](const HttpRequest&, HttpResponse& resp) -> void {
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "GET /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));

        const std::string responseStr(receivedData.begin(), receivedData.end());
        EXPECT_TRUE(responseStr.contains("HTTP/1.1 200"));
    }

    TEST(HttpSessionTest, ResponseContainsBody)
    {
        const auto router = std::make_shared<HttpRouter>();
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/test"},
                               [](const HttpRequest&, HttpResponse& resp) -> void {
                                   resp.Ok();
                                   resp.Body("Hello, World!");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "GET /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));

        const std::string responseStr(receivedData.begin(), receivedData.end());
        EXPECT_TRUE(responseStr.contains("Hello, World!"));
    }

    // === Edge case: request with no Host header (HTTP/1.1 requires it) ===

    TEST(HttpSessionTest, RequestWithoutHostHeader)
    {
        HttpProtocolSession session(MakeDefaultRouter());
        session.SetMaxRequestSize(DefaultRequestSize);

        // HTTP/1.1 requires Host header; Beast parser should reject it
        constexpr std::string_view request = "GET / HTTP/1.1\r\n\r\n";
        EXPECT_FALSE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
    }

    // === Edge case: very large number of headers ===

    TEST(HttpSessionTest, RequestWithManyHeaders)
    {
        const auto router = std::make_shared<HttpRouter>();
        bool handlerCalled = false;
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/test"},
                               [&handlerCalled](const HttpRequest&, HttpResponse& resp) -> void {
                                   handlerCalled = true;
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(LargeRequestSize);

        std::string request = "GET /test HTTP/1.1\r\nHost: example.com\r\n";
        for (int i = 0; i < HeaderCount; ++i)
        {
            request += "X-Custom-Header-" + std::to_string(i) + ": value" + std::to_string(i)
                     + "\r\n";
        }
        request += "\r\n";

        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_TRUE(handlerCalled);
    }

    // === Edge case: request with query string ===
    // Note: HttpRouter matches exact path, so /search?q=... won't match /search route.
    // Register the full target including query string to verify the target is preserved.

    TEST(HttpSessionTest, RequestWithQueryString)
    {
        const auto router = std::make_shared<HttpRouter>();
        std::string capturedTarget;
        (void)router->AddRoute(
            {.Method = HttpMethod::Get, .Path = "/search?q=hello+world&lang=en"},
            [&capturedTarget](const HttpRequest& req, HttpResponse& resp) -> void {
                capturedTarget = std::string(req.Path());
                resp.Ok();
                resp.Body("OK");
            });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view
            request = "GET /search?q=hello+world&lang=en HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_EQ(capturedTarget, "/search?q=hello+world&lang=en");
    }

    // === Edge case: request with fragment ===
    // Note: HttpRouter matches exact path, so /page#section1 won't match /page route.
    // Register the full target including fragment to verify the target is preserved.

    TEST(HttpSessionTest, RequestWithFragment)
    {
        const auto router = std::make_shared<HttpRouter>();
        std::string capturedTarget;
        (void)router->AddRoute(
            {.Method = HttpMethod::Get, .Path = "/page#section1"},
            [&capturedTarget](const HttpRequest& req, HttpResponse& resp) -> void {
                capturedTarget = std::string(req.Path());
                resp.Ok();
                resp.Body("OK");
            });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view
            request = "GET /page#section1 HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_EQ(capturedTarget, "/page#section1");
    }

    // === Edge case: POST with empty body ===

    TEST(HttpSessionTest, PostWithEmptyBody)
    {
        const auto router = std::make_shared<HttpRouter>();
        std::string capturedBody;
        (void)router->AddRoute({.Method = HttpMethod::Post, .Path = "/empty"},
                               [&capturedBody](const HttpRequest& req, HttpResponse& resp) -> void {
                                   capturedBody = req.Body();
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view
            request = "POST /empty HTTP/1.1\r\nHost: example.com\r\nContent-Length: 0\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_EQ(capturedBody, "");
    }

    // === Edge case: HEAD request ===

    TEST(HttpSessionTest, HeadRequest)
    {
        const auto router = std::make_shared<HttpRouter>();
        bool handlerCalled = false;
        (void)router->AddRoute({.Method = HttpMethod::Head, .Path = "/test"},
                               [&handlerCalled](const HttpRequest&, HttpResponse& resp) -> void {
                                   handlerCalled = true;
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "HEAD /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_TRUE(handlerCalled);
    }

    // === Edge case: OPTIONS request ===

    TEST(HttpSessionTest, OptionsRequest)
    {
        const auto router = std::make_shared<HttpRouter>();
        bool handlerCalled = false;
        (void)router->AddRoute({.Method = HttpMethod::Options, .Path = "/test"},
                               [&handlerCalled](const HttpRequest&, HttpResponse& resp) -> void {
                                   handlerCalled = true;
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "OPTIONS /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_TRUE(handlerCalled);
    }

    // === Edge case: DELETE request ===

    TEST(HttpSessionTest, DeleteRequest)
    {
        const auto router = std::make_shared<HttpRouter>();
        bool handlerCalled = false;
        (void)router->AddRoute({.Method = HttpMethod::Delete, .Path = "/test"},
                               [&handlerCalled](const HttpRequest&, HttpResponse& resp) -> void {
                                   handlerCalled = true;
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "DELETE /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_TRUE(handlerCalled);
    }

    // === Edge case: PUT request ===

    TEST(HttpSessionTest, PutRequest)
    {
        const auto router = std::make_shared<HttpRouter>();
        bool handlerCalled = false;
        (void)router->AddRoute({.Method = HttpMethod::Put, .Path = "/test"},
                               [&handlerCalled](const HttpRequest&, HttpResponse& resp) -> void {
                                   handlerCalled = true;
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "PUT /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_TRUE(handlerCalled);
    }

    // === Edge case: PATCH request ===

    TEST(HttpSessionTest, PatchRequest)
    {
        const auto router = std::make_shared<HttpRouter>();
        bool handlerCalled = false;
        (void)router->AddRoute({.Method = HttpMethod::Patch, .Path = "/test"},
                               [&handlerCalled](const HttpRequest&, HttpResponse& resp) -> void {
                                   handlerCalled = true;
                                   resp.Ok();
                                   resp.Body("OK");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "PATCH /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));
        EXPECT_TRUE(handlerCalled);
    }

    // === Edge case: response with custom status codes ===

    TEST(HttpSessionTest, ResponseWithCustomStatusCode)
    {
        const auto router = std::make_shared<HttpRouter>();
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/created"},
                               [](const HttpRequest&, HttpResponse& resp) -> void {
                                   resp.Created();
                                   resp.Body("Created");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "GET /created HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));

        const std::string responseStr(receivedData.begin(), receivedData.end());
        EXPECT_TRUE(responseStr.contains("201"));
    }

    // === Edge case: response with custom headers ===

    TEST(HttpSessionTest, ResponseWithCustomHeaders)
    {
        const auto router = std::make_shared<HttpRouter>();
        (void)router->AddRoute({.Method = HttpMethod::Get, .Path = "/headers"},
                               [](const HttpRequest&, HttpResponse& resp) -> void {
                                   resp.Ok();
                                   resp.Set(HttpField::ContentType, "application/json");
                                   resp.Set("X-Custom-Header", "custom-value");
                                   resp.Body("{}");
                               });

        HttpProtocolSession session(router);
        std::vector<std::byte> receivedData;
        session.SetSendCallback([&receivedData](std::vector<std::byte>&& data) -> Result {
            receivedData = std::move(data);
            return Result {};
        });
        session.SetMaxRequestSize(DefaultRequestSize);

        constexpr std::string_view request = "GET /headers HTTP/1.1\r\nHost: example.com\r\n\r\n";
        EXPECT_TRUE(session.OnBytes(std::as_bytes(std::span(request.data(), request.size()))));

        const std::string responseStr(receivedData.begin(), receivedData.end());
        EXPECT_TRUE(responseStr.contains("application/json"));
        EXPECT_TRUE(responseStr.contains("custom-value"));
    }

}   // namespace Flux::Test
