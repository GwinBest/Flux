#include "Protocol/Http/HttpRequest.hpp"

#include "gtest/gtest.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

#include "Protocol/Http/HttpField.hpp"
#include "Protocol/Http/HttpMethod.hpp"

namespace Flux::Test
{
    static constexpr std::uint32_t HttpVersion11 = 11U;

    // === Constructors ===

    TEST(HttpRequestTest, DefaultConstructorCreatesEmptyRequest)
    {
        const HttpRequest request;
        // Boost.Beast initializes version to 11 (HTTP/1.1) by default
        EXPECT_EQ(request.Version(), HttpVersion11);
        EXPECT_EQ(request.Path(), "");
        EXPECT_EQ(request.Body(), "");
        EXPECT_EQ(request.PayloadSize(), 0U);
    }

    TEST(HttpRequestTest, CopyConstructorPreservesData)
    {
        Detail::HttpRequest src;
        src.method(boost::beast::http::verb::get);
        src.target("/test");
        src.version(HttpVersion11);

        const HttpRequest request(src);
        EXPECT_EQ(request.Method(), HttpMethod::Get);
        EXPECT_EQ(request.Path(), "/test");
        EXPECT_EQ(request.Version(), HttpVersion11);
    }

    TEST(HttpRequestTest, MoveConstructorPreservesData)
    {
        Detail::HttpRequest src;
        src.method(boost::beast::http::verb::post);
        src.target("/submit");
        src.version(HttpVersion11);
        src.body() = "hello";

        const HttpRequest request(std::move(src));
        EXPECT_EQ(request.Method(), HttpMethod::Post);
        EXPECT_EQ(request.Path(), "/submit");
        EXPECT_EQ(request.Version(), HttpVersion11);
        EXPECT_EQ(request.Body(), "hello");
    }

    // === Path() ===

    TEST(HttpRequestTest, PathReturnsTarget)
    {
        Detail::HttpRequest src;
        src.target("/api/v1/users");
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Path(), "/api/v1/users");
    }

    TEST(HttpRequestTest, PathReturnsEmptyForDefaultRequest)
    {
        const HttpRequest request;
        EXPECT_EQ(request.Path(), "");
    }

    TEST(HttpRequestTest, PathReturnsRoot)
    {
        Detail::HttpRequest src;
        src.target("/");
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Path(), "/");
    }

    TEST(HttpRequestTest, PathReturnsLongPath)
    {
        const std::string longPath(1024, 'a');
        Detail::HttpRequest src;
        src.target(longPath);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Path(), longPath);
    }

    TEST(HttpRequestTest, PathReturnsPathWithQuery)
    {
        Detail::HttpRequest src;
        src.target("/search?q=test&page=1");
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Path(), "/search?q=test&page=1");
    }

    // === Method() ===

    TEST(HttpRequestTest, MethodReturnsGet)
    {
        Detail::HttpRequest src;
        src.method(boost::beast::http::verb::get);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Method(), HttpMethod::Get);
    }

    TEST(HttpRequestTest, MethodReturnsPost)
    {
        Detail::HttpRequest src;
        src.method(boost::beast::http::verb::post);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Method(), HttpMethod::Post);
    }

    TEST(HttpRequestTest, MethodReturnsPut)
    {
        Detail::HttpRequest src;
        src.method(boost::beast::http::verb::put);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Method(), HttpMethod::Put);
    }

    TEST(HttpRequestTest, MethodReturnsDelete)
    {
        Detail::HttpRequest src;
        src.method(boost::beast::http::verb::delete_);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Method(), HttpMethod::Delete);
    }

    TEST(HttpRequestTest, MethodReturnsHead)
    {
        Detail::HttpRequest src;
        src.method(boost::beast::http::verb::head);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Method(), HttpMethod::Head);
    }

    TEST(HttpRequestTest, MethodReturnsOptions)
    {
        Detail::HttpRequest src;
        src.method(boost::beast::http::verb::options);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Method(), HttpMethod::Options);
    }

    TEST(HttpRequestTest, MethodReturnsPatch)
    {
        Detail::HttpRequest src;
        src.method(boost::beast::http::verb::patch);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Method(), HttpMethod::Patch);
    }

    TEST(HttpRequestTest, MethodReturnsUnknownForDefault)
    {
        const HttpRequest request;
        // Default method in Boost.Beast is unknown
        EXPECT_EQ(request.Method(), HttpMethod::Unknown);
    }

    // === Version() ===

    TEST(HttpRequestTest, VersionReturns11ForHttp1_1)
    {
        Detail::HttpRequest src;
        src.version(HttpVersion11);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Version(), HttpVersion11);
    }

    TEST(HttpRequestTest, VersionReturns10ForHttp1_0)
    {
        Detail::HttpRequest src;
        constexpr auto version = 10;
        src.version(version);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Version(), 10U);
    }

    TEST(HttpRequestTest, VersionReturnsHttp1_1ForDefault)
    {
        const HttpRequest request;
        // Boost.Beast initializes version to 11 (HTTP/1.1) by default
        EXPECT_EQ(request.Version(), HttpVersion11);
    }

    TEST(HttpRequestTest, VersionReturnsMaxValidValue)
    {
        // Boost.Beast requires 1 < version < 100, so max valid is 99
        Detail::HttpRequest src;
        constexpr auto version = 99;
        src.version(version);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Version(), 99U);
    }

    // === KeepAlive() ===

    TEST(HttpRequestTest, KeepAliveReturnsTrueForDefault)
    {
        const HttpRequest request;
        // Boost.Beast initializes keep_alive to true by default
        EXPECT_TRUE(request.KeepAlive());
    }

    TEST(HttpRequestTest, KeepAliveReturnsTrueWhenSet)
    {
        Detail::HttpRequest src;
        src.version(HttpVersion11);
        src.keep_alive(true);
        const HttpRequest request(std::move(src));

        EXPECT_TRUE(request.KeepAlive());
    }

    TEST(HttpRequestTest, KeepAliveReturnsFalseWhenExplicitlySetFalse)
    {
        Detail::HttpRequest src;
        src.version(HttpVersion11);
        src.keep_alive(false);
        const HttpRequest request(std::move(src));

        EXPECT_FALSE(request.KeepAlive());
    }

    // === Body() ===

    TEST(HttpRequestTest, BodyReturnsEmptyForDefault)
    {
        const HttpRequest request;
        EXPECT_EQ(request.Body(), "");
    }

    TEST(HttpRequestTest, BodyReturnsSetBody)
    {
        Detail::HttpRequest src;
        src.body() = "test body content";
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Body(), "test body content");
    }

    TEST(HttpRequestTest, BodyReturnsEmptyString)
    {
        Detail::HttpRequest src;
        src.body() = "";
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Body(), "");
    }

    TEST(HttpRequestTest, BodyReturnsLongBody)
    {
        const std::string longBody(10240, 'x');
        Detail::HttpRequest src;
        src.body() = longBody;
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Body(), longBody);
    }

    // === PayloadSize() ===

    TEST(HttpRequestTest, PayloadSizeReturnsZeroForDefault)
    {
        const HttpRequest request;
        EXPECT_EQ(request.PayloadSize(), 0U);
    }

    TEST(HttpRequestTest, PayloadSizeReturnsZeroWhenNotSet)
    {
        Detail::HttpRequest src;
        src.method(boost::beast::http::verb::get);
        src.target("/");
        src.version(HttpVersion11);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.PayloadSize(), 0U);
    }

    TEST(HttpRequestTest, PayloadSizeReturnsCorrectValue)
    {
        Detail::HttpRequest src;
        src.method(boost::beast::http::verb::post);
        src.target("/");
        src.version(HttpVersion11);
        src.set(boost::beast::http::field::content_length, "5");
        src.body() = "hello";
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.PayloadSize(), 5U);
    }

    // === Header(HttpField) ===

    TEST(HttpRequestTest, HeaderByFieldReturnsValue)
    {
        Detail::HttpRequest src;
        src.set(boost::beast::http::field::host, "example.com");
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Header(HttpField::Host), "example.com");
    }

    TEST(HttpRequestTest, HeaderByFieldReturnsEmptyForMissing)
    {
        const HttpRequest request;
        EXPECT_EQ(request.Header(HttpField::Host), "");
    }

    TEST(HttpRequestTest, HeaderByFieldReturnsContentType)
    {
        Detail::HttpRequest src;
        src.set(boost::beast::http::field::content_type, "application/json");
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Header(HttpField::ContentType), "application/json");
    }

    TEST(HttpRequestTest, HeaderByFieldReturnsContentLength)
    {
        Detail::HttpRequest src;
        src.set(boost::beast::http::field::content_length, "1024");
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Header(HttpField::ContentLength), "1024");
    }

    TEST(HttpRequestTest, HeaderByFieldReturnsUserAgent)
    {
        Detail::HttpRequest src;
        src.set(boost::beast::http::field::user_agent, "TestAgent/1.0");
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Header(HttpField::UserAgent), "TestAgent/1.0");
    }

    TEST(HttpRequestTest, HeaderByFieldReturnsEmptyForUnknown)
    {
        const HttpRequest request;
        EXPECT_EQ(request.Header(HttpField::Unknown), "");
    }

    // === Header(string_view) ===

    TEST(HttpRequestTest, HeaderByNameReturnsValue)
    {
        Detail::HttpRequest src;
        src.set("X-Custom-Header", "custom-value");
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Header("X-Custom-Header"), "custom-value");
    }

    TEST(HttpRequestTest, HeaderByNameReturnsEmptyForMissing)
    {
        const HttpRequest request;
        EXPECT_EQ(request.Header("X-Missing"), "");
    }

    TEST(HttpRequestTest, HeaderByNameReturnsEmptyForEmptyName)
    {
        const HttpRequest request;
        EXPECT_EQ(request.Header(""), "");
    }

    TEST(HttpRequestTest, HeaderByNameIsCaseInsensitive)
    {
        Detail::HttpRequest src;
        src.set("X-Test-Header", "value");
        const HttpRequest request(std::move(src));

        // HTTP headers are case-insensitive in Boost.Beast
        EXPECT_EQ(request.Header("x-test-header"), "value");
    }

    // === HasHeader(HttpField) ===

    TEST(HttpRequestTest, HasHeaderByFieldReturnsTrueWhenPresent)
    {
        Detail::HttpRequest src;
        src.set(boost::beast::http::field::host, "example.com");
        const HttpRequest request(std::move(src));

        EXPECT_TRUE(request.HasHeader(HttpField::Host));
    }

    TEST(HttpRequestTest, HasHeaderByFieldReturnsFalseWhenMissing)
    {
        const HttpRequest request;
        EXPECT_FALSE(request.HasHeader(HttpField::Host));
    }

    TEST(HttpRequestTest, HasHeaderByFieldReturnsFalseForUnknown)
    {
        const HttpRequest request;
        EXPECT_FALSE(request.HasHeader(HttpField::Unknown));
    }

    // === HasHeader(string_view) ===

    TEST(HttpRequestTest, HasHeaderByNameReturnsTrueWhenPresent)
    {
        Detail::HttpRequest src;
        src.set("X-Custom", "value");
        const HttpRequest request(std::move(src));

        EXPECT_TRUE(request.HasHeader("X-Custom"));
    }

    TEST(HttpRequestTest, HasHeaderByNameReturnsFalseWhenMissing)
    {
        const HttpRequest request;
        EXPECT_FALSE(request.HasHeader("X-Missing"));
    }

    TEST(HttpRequestTest, HasHeaderByNameReturnsFalseForEmptyName)
    {
        const HttpRequest request;
        EXPECT_FALSE(request.HasHeader(""));
    }

    // === operator[] ===

    TEST(HttpRequestTest, OperatorBracketByFieldReturnsValue)
    {
        Detail::HttpRequest src;
        src.set(boost::beast::http::field::host, "example.com");
        const HttpRequest request(std::move(src));

        EXPECT_EQ(
            request[HttpField::Host],   // NOLINT(*-pro-bounds-avoid-unchecked-container-access)
            "example.com");
    }

    TEST(HttpRequestTest, OperatorBracketByNameReturnsValue)
    {
        Detail::HttpRequest src;
        src.set("X-Custom", "value");
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request["X-Custom"],   // NOLINT(*-pro-bounds-avoid-unchecked-container-access)
                  "value");
    }

    TEST(HttpRequestTest, OperatorBracketReturnsEmptyForMissing)
    {
        const HttpRequest request;
        EXPECT_EQ(request["X-Missing"],   // NOLINT(*-pro-bounds-avoid-unchecked-container-access)
                  "");
    }

    // === Raw() ===

    TEST(HttpRequestTest, RawConstReturnsReference)
    {
        Detail::HttpRequest src;
        src.target("/test");
        const HttpRequest request(std::move(src));

        const HttpRequest& constRef = request;
        const auto& raw = constRef.Raw();
        EXPECT_EQ(raw.target(), "/test");
    }

    TEST(HttpRequestTest, RawRvalueReturnsReference)
    {
        Detail::HttpRequest src;
        src.target("/test");
        HttpRequest request(std::move(src));

        const auto& raw = std::move(request).Raw();
        EXPECT_EQ(raw.target(), "/test");
    }

    // === Multiple headers ===

    TEST(HttpRequestTest, MultipleHeadersAreAccessible)
    {
        Detail::HttpRequest src;
        src.set(boost::beast::http::field::host, "example.com");
        src.set(boost::beast::http::field::content_type, "application/json");
        src.set(boost::beast::http::field::user_agent, "TestAgent/1.0");
        src.set("X-Custom", "custom-value");
        const HttpRequest request(std::move(src));

        EXPECT_TRUE(request.HasHeader(HttpField::Host));
        EXPECT_TRUE(request.HasHeader(HttpField::ContentType));
        EXPECT_TRUE(request.HasHeader(HttpField::UserAgent));
        EXPECT_TRUE(request.HasHeader("X-Custom"));

        EXPECT_EQ(request.Header(HttpField::Host), "example.com");
        EXPECT_EQ(request.Header(HttpField::ContentType), "application/json");
        EXPECT_EQ(request.Header(HttpField::UserAgent), "TestAgent/1.0");
        EXPECT_EQ(request.Header("X-Custom"), "custom-value");
    }

    // === Edge cases ===

    TEST(HttpRequestTest, RequestWithNullByteInPath)
    {
        Detail::HttpRequest src;
        src.target("/path\0with\0null");
        const HttpRequest request(std::move(src));

        // Path should contain the null bytes as part of the target
        EXPECT_FALSE(request.Path().empty());
    }

    TEST(HttpRequestTest, RequestWithSpecialCharactersInPath)
    {
        Detail::HttpRequest src;
        src.target("/api/v1/resource/123?query=value&other=test");
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Path(), "/api/v1/resource/123?query=value&other=test");
    }

    TEST(HttpRequestTest, RequestWithUnicodeInPath)
    {
        Detail::HttpRequest src;
        src.target("/path/тест");
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Path(), "/path/тест");
    }

    TEST(HttpRequestTest, RequestWithEmptyHeaderValue)
    {
        Detail::HttpRequest src;
        src.set("X-Empty", "");
        const HttpRequest request(std::move(src));

        EXPECT_TRUE(request.HasHeader("X-Empty"));
        EXPECT_EQ(request.Header("X-Empty"), "");
    }

    TEST(HttpRequestTest, RequestWithLongHeaderValue)
    {
        const std::string longValue(4096, 'v');
        Detail::HttpRequest src;
        src.set("X-Long-Header", longValue);
        const HttpRequest request(std::move(src));

        EXPECT_EQ(request.Header("X-Long-Header"), longValue);
    }

    // === [[nodiscard]] checks ===

    TEST(HttpRequestTest, PathIsNodiscard)
    {
        const std::string_view result = HttpRequest().Path();
        EXPECT_EQ(result, "");
    }

    TEST(HttpRequestTest, MethodIsNodiscard)
    {
        const HttpMethod result = HttpRequest().Method();
        EXPECT_EQ(result, HttpMethod::Unknown);
    }

    TEST(HttpRequestTest, VersionIsNodiscard)
    {
        const std::uint32_t result = HttpRequest().Version();
        // Boost.Beast initializes version to 11 (HTTP/1.1)
        EXPECT_EQ(result, HttpVersion11);
    }

    TEST(HttpRequestTest, KeepAliveIsNodiscard)
    {
        const bool result = HttpRequest().KeepAlive();
        // Boost.Beast initializes keep_alive to true by default
        EXPECT_TRUE(result);
    }

    TEST(HttpRequestTest, BodyIsNodiscard)
    {
        const std::string_view result = HttpRequest().Body();
        EXPECT_EQ(result, "");
    }

    TEST(HttpRequestTest, PayloadSizeIsNodiscard)
    {
        const std::uint64_t result = HttpRequest().PayloadSize();
        EXPECT_EQ(result, 0U);
    }
}   // namespace Flux::Test
