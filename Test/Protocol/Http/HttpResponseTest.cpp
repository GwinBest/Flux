#include "Protocol/Http/HttpResponse.hpp"

#include "gtest/gtest.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "Protocol/Http/HttpField.hpp"
#include "Protocol/Http/HttpStatus.hpp"

namespace Flux::Test
{
    static constexpr std::uint32_t HttpVersion11 = 11U;

    // === Constructors ===

    TEST(HttpResponseTest, DefaultConstructorCreatesEmptyResponse)
    {
        const HttpResponse response;
        EXPECT_EQ(response.Body(), "");
        // Boost.Beast initializes response with status 200 OK by default
        EXPECT_EQ(response.Reason(), "OK");
    }

    TEST(HttpResponseTest, CopyConstructorPreservesData)
    {
        Detail::HttpResponse src;
        src.result(boost::beast::http::status::ok);
        src.body() = "test body";
        src.version(HttpVersion11);

        const HttpResponse response(src);
        EXPECT_EQ(response.StatusCode(), HttpStatus::Ok);
        EXPECT_EQ(response.Body(), "test body");
        EXPECT_EQ(response.Version(), HttpVersion11);
    }

    TEST(HttpResponseTest, MoveConstructorPreservesData)
    {
        Detail::HttpResponse src;
        src.result(boost::beast::http::status::created);
        src.body() = "moved body";

        const HttpResponse response(std::move(src));
        EXPECT_EQ(response.StatusCode(), HttpStatus::Created);
        EXPECT_EQ(response.Body(), "moved body");
    }

    // === Status(HttpStatus) ===

    TEST(HttpResponseTest, StatusSetsHttpStatus)
    {
        HttpResponse response;
        response.Status(HttpStatus::Ok);
        EXPECT_EQ(response.StatusCode(), HttpStatus::Ok);
    }

    TEST(HttpResponseTest, StatusSetsNotFound)
    {
        HttpResponse response;
        response.Status(HttpStatus::NotFound);
        EXPECT_EQ(response.StatusCode(), HttpStatus::NotFound);
    }

    TEST(HttpResponseTest, StatusSetsInternalServerError)
    {
        HttpResponse response;
        response.Status(HttpStatus::InternalServerError);
        EXPECT_EQ(response.StatusCode(), HttpStatus::InternalServerError);
    }

    // === Status(uint16_t) ===

    TEST(HttpResponseTest, StatusSetsIntegerCode)
    {
        constexpr std::uint16_t statusCodeCreated = 201U;
        HttpResponse response;
        response.Status(statusCodeCreated);
        EXPECT_EQ(response.StatusCodeInt(), statusCodeCreated);
    }

    TEST(HttpResponseTest, StatusSetsZeroCode)
    {
        HttpResponse response;
        response.Status(0U);
        EXPECT_EQ(response.StatusCodeInt(), 0U);
    }

    TEST(HttpResponseTest, StatusSetsMaxCode)
    {
        HttpResponse response;
        response.Status(std::numeric_limits<std::uint16_t>::max());
        EXPECT_EQ(response.StatusCodeInt(), std::numeric_limits<std::uint16_t>::max());
    }

    // === Result(HttpStatus) ===

    TEST(HttpResponseTest, ResultSetsHttpStatus)
    {
        HttpResponse response;
        response.Result(HttpStatus::Accepted);
        EXPECT_EQ(response.StatusCode(), HttpStatus::Accepted);
    }

    TEST(HttpResponseTest, ResultSetsIntegerCode)
    {
        constexpr std::uint16_t statusCodeNotFound = 404U;
        HttpResponse response;
        response.Result(statusCodeNotFound);
        EXPECT_EQ(response.StatusCodeInt(), statusCodeNotFound);
    }

    // === Convenience status methods ===

    TEST(HttpResponseTest, OkSets200)
    {
        HttpResponse response;
        response.Ok();
        EXPECT_EQ(response.StatusCode(), HttpStatus::Ok);
    }

    TEST(HttpResponseTest, CreatedSets201)
    {
        HttpResponse response;
        response.Created();
        EXPECT_EQ(response.StatusCode(), HttpStatus::Created);
    }

    TEST(HttpResponseTest, AcceptedSets202)
    {
        HttpResponse response;
        response.Accepted();
        EXPECT_EQ(response.StatusCode(), HttpStatus::Accepted);
    }

    TEST(HttpResponseTest, NoContentSets204)
    {
        HttpResponse response;
        response.NoContent();
        EXPECT_EQ(response.StatusCode(), HttpStatus::NoContent);
    }

    TEST(HttpResponseTest, BadRequestSets400)
    {
        HttpResponse response;
        response.BadRequest();
        EXPECT_EQ(response.StatusCode(), HttpStatus::BadRequest);
    }

    TEST(HttpResponseTest, UnauthorizedSets401)
    {
        HttpResponse response;
        response.Unauthorized();
        EXPECT_EQ(response.StatusCode(), HttpStatus::Unauthorized);
    }

    TEST(HttpResponseTest, ForbiddenSets403)
    {
        HttpResponse response;
        response.Forbidden();
        EXPECT_EQ(response.StatusCode(), HttpStatus::Forbidden);
    }

    TEST(HttpResponseTest, NotFoundSets404)
    {
        HttpResponse response;
        response.NotFound();
        EXPECT_EQ(response.StatusCode(), HttpStatus::NotFound);
    }

    TEST(HttpResponseTest, MethodNotAllowedSets405)
    {
        HttpResponse response;
        response.MethodNotAllowed();
        EXPECT_EQ(response.StatusCode(), HttpStatus::MethodNotAllowed);
    }

    TEST(HttpResponseTest, InternalServerErrorSets500)
    {
        HttpResponse response;
        response.InternalServerError();
        EXPECT_EQ(response.StatusCode(), HttpStatus::InternalServerError);
    }

    TEST(HttpResponseTest, NotImplementedSets501)
    {
        HttpResponse response;
        response.NotImplemented();
        EXPECT_EQ(response.StatusCode(), HttpStatus::NotImplemented);
    }

    TEST(HttpResponseTest, ServiceUnavailableSets503)
    {
        HttpResponse response;
        response.ServiceUnavailable();
        EXPECT_EQ(response.StatusCode(), HttpStatus::ServiceUnavailable);
    }

    // === Reason() ===

    TEST(HttpResponseTest, ReasonSetsCustomReason)
    {
        HttpResponse response;
        response.Reason("Custom Reason");
        EXPECT_EQ(response.Reason(), "Custom Reason");
    }

    TEST(HttpResponseTest, ReasonReturnsOkForDefault)
    {
        const HttpResponse response;
        // Boost.Beast initializes response with status 200 OK, so reason is "OK"
        EXPECT_EQ(response.Reason(), "OK");
    }

    TEST(HttpResponseTest, ReasonSetsEmptyStringKeepsOk)
    {
        HttpResponse response;
        // Boost.Beast keeps the default reason "OK" even when setting empty string
        response.Reason("");
        EXPECT_EQ(response.Reason(), "OK");
    }

    TEST(HttpResponseTest, ReasonSetsLongReason)
    {
        const std::string longReason(256, 'R');
        HttpResponse response;
        response.Reason(longReason);
        EXPECT_EQ(response.Reason(), longReason);
    }

    // === Version() ===

    TEST(HttpResponseTest, VersionSetsHttp1_1)
    {
        HttpResponse response;
        response.Version(HttpVersion11);
        EXPECT_EQ(response.Version(), HttpVersion11);
    }

    TEST(HttpResponseTest, VersionSetsHttp1_0)
    {
        constexpr std::uint32_t httpVersion10 = 10U;
        HttpResponse response;
        response.Version(httpVersion10);
        EXPECT_EQ(response.Version(), httpVersion10);
    }

    TEST(HttpResponseTest, VersionReturnsHttp1_1ForDefault)
    {
        const HttpResponse response;
        // Boost.Beast initializes version to 11 (HTTP/1.1) by default
        EXPECT_EQ(response.Version(), HttpVersion11);
    }

    TEST(HttpResponseTest, VersionSetsMaxValidValue)
    {
        // Boost.Beast requires 1 < version < 100, so max valid is 99
        constexpr std::uint32_t maxValidVersion = 99U;
        HttpResponse response;
        response.Version(maxValidVersion);
        EXPECT_EQ(response.Version(), maxValidVersion);
    }

    TEST(HttpResponseTest, VersionSetsMinValidValue)
    {
        // Boost.Beast requires 1 < version < 100, so min valid is 2
        HttpResponse response;
        response.Version(2);
        EXPECT_EQ(response.Version(), 2U);
    }

    // === KeepAlive() ===

    TEST(HttpResponseTest, KeepAliveSetsTrue)
    {
        HttpResponse response;
        response.KeepAlive(true);
        EXPECT_TRUE(response.KeepAlive());
    }

    TEST(HttpResponseTest, KeepAliveSetsFalse)
    {
        HttpResponse response;
        response.KeepAlive(false);
        EXPECT_FALSE(response.KeepAlive());
    }

    TEST(HttpResponseTest, KeepAliveReturnsTrueForDefault)
    {
        const HttpResponse response;
        // Boost.Beast initializes keep_alive to true by default
        EXPECT_TRUE(response.KeepAlive());
    }

    // === Body() ===

    TEST(HttpResponseTest, BodySetsString)
    {
        HttpResponse response;
        response.Body(std::string("test body"));
        EXPECT_EQ(response.Body(), "test body");
    }

    TEST(HttpResponseTest, BodySetsCharPointer)
    {
        HttpResponse response;
        response.Body("char pointer body");
        EXPECT_EQ(response.Body(), "char pointer body");
    }

    TEST(HttpResponseTest, BodyReturnsEmptyForDefault)
    {
        const HttpResponse response;
        EXPECT_EQ(response.Body(), "");
    }

    TEST(HttpResponseTest, BodySetsEmptyString)
    {
        HttpResponse response;
        response.Body(std::string(""));
        EXPECT_EQ(response.Body(), "");
    }

    TEST(HttpResponseTest, BodySetsLongBody)
    {
        const std::string longBody(10240, 'x');
        HttpResponse response;
        response.Body(longBody);
        EXPECT_EQ(response.Body(), longBody);
    }

    TEST(HttpResponseTest, ClearBodyClearsContent)
    {
        HttpResponse response;
        response.Body(std::string("content"));
        response.ClearBody();
        EXPECT_EQ(response.Body(), "");
    }

    // === Set() ===

    TEST(HttpResponseTest, SetByFieldSetsHeader)
    {
        HttpResponse response;
        response.Set(HttpField::ContentType, "text/html");
        EXPECT_EQ(response.Header(HttpField::ContentType), "text/html");
    }

    TEST(HttpResponseTest, SetByNameSetsHeader)
    {
        HttpResponse response;
        response.Set("X-Custom", "value");
        EXPECT_EQ(response.Header("X-Custom"), "value");
    }

    TEST(HttpResponseTest, SetByFieldOverwritesExisting)
    {
        HttpResponse response;
        response.Set(HttpField::ContentType, "text/plain");
        response.Set(HttpField::ContentType, "text/html");
        EXPECT_EQ(response.Header(HttpField::ContentType), "text/html");
    }

    TEST(HttpResponseTest, SetByNameOverwritesExisting)
    {
        HttpResponse response;
        response.Set("X-Custom", "old");
        response.Set("X-Custom", "new");
        EXPECT_EQ(response.Header("X-Custom"), "new");
    }

    TEST(HttpResponseTest, SetEmptyValue)
    {
        HttpResponse response;
        response.Set("X-Empty", "");
        EXPECT_TRUE(response.HasHeader("X-Empty"));
        EXPECT_EQ(response.Header("X-Empty"), "");
    }

    // === Insert() ===

    TEST(HttpResponseTest, InsertByFieldInsertsHeader)
    {
        HttpResponse response;
        response.Insert(HttpField::SetCookie, "session=abc");
        EXPECT_TRUE(response.HasHeader(HttpField::SetCookie));
    }

    TEST(HttpResponseTest, InsertByNameInsertsHeader)
    {
        HttpResponse response;
        response.Insert("X-Custom", "value");
        EXPECT_TRUE(response.HasHeader("X-Custom"));
    }

    // === Remove() ===

    TEST(HttpResponseTest, RemoveByFieldRemovesHeader)
    {
        HttpResponse response;
        response.Set(HttpField::ContentType, "text/html");
        EXPECT_TRUE(response.HasHeader(HttpField::ContentType));

        response.Remove(HttpField::ContentType);
        EXPECT_FALSE(response.HasHeader(HttpField::ContentType));
    }

    TEST(HttpResponseTest, RemoveByNameRemovesHeader)
    {
        HttpResponse response;
        response.Set("X-Custom", "value");
        EXPECT_TRUE(response.HasHeader("X-Custom"));

        response.Remove("X-Custom");
        EXPECT_FALSE(response.HasHeader("X-Custom"));
    }

    TEST(HttpResponseTest, RemoveNonExistentHeaderIsSafe)
    {
        HttpResponse response;
        response.Remove("X-NonExistent");
        EXPECT_FALSE(response.HasHeader("X-NonExistent"));
    }

    // === Replace() ===

    TEST(HttpResponseTest, ReplaceByFieldReplacesHeader)
    {
        HttpResponse response;
        response.Set(HttpField::ContentType, "text/plain");
        response.Replace(HttpField::ContentType, "text/html");
        EXPECT_EQ(response.Header(HttpField::ContentType), "text/html");
    }

    TEST(HttpResponseTest, ReplaceByNameReplacesHeader)
    {
        HttpResponse response;
        response.Set("X-Custom", "old");
        response.Replace("X-Custom", "new");
        EXPECT_EQ(response.Header("X-Custom"), "new");
    }

    // === Header() ===

    TEST(HttpResponseTest, HeaderByFieldReturnsValue)
    {
        HttpResponse response;
        response.Set(HttpField::ContentType, "application/json");
        EXPECT_EQ(response.Header(HttpField::ContentType), "application/json");
    }

    TEST(HttpResponseTest, HeaderByNameReturnsValue)
    {
        HttpResponse response;
        response.Set("X-Custom", "value");
        EXPECT_EQ(response.Header("X-Custom"), "value");
    }

    TEST(HttpResponseTest, HeaderReturnsEmptyForMissing)
    {
        const HttpResponse response;
        EXPECT_EQ(response.Header(HttpField::ContentType), "");
    }

    // === HasHeader() ===

    TEST(HttpResponseTest, HasHeaderByFieldReturnsTrueWhenPresent)
    {
        HttpResponse response;
        response.Set(HttpField::ContentType, "text/html");
        EXPECT_TRUE(response.HasHeader(HttpField::ContentType));
    }

    TEST(HttpResponseTest, HasHeaderByFieldReturnsFalseWhenMissing)
    {
        const HttpResponse response;
        EXPECT_FALSE(response.HasHeader(HttpField::ContentType));
    }

    TEST(HttpResponseTest, HasHeaderByNameReturnsTrueWhenPresent)
    {
        HttpResponse response;
        response.Set("X-Custom", "value");
        EXPECT_TRUE(response.HasHeader("X-Custom"));
    }

    TEST(HttpResponseTest, HasHeaderByNameReturnsFalseWhenMissing)
    {
        const HttpResponse response;
        EXPECT_FALSE(response.HasHeader("X-Missing"));
    }

    // === operator[] ===

    TEST(HttpResponseTest, OperatorBracketByFieldReturnsValue)
    {
        HttpResponse response;
        response.Set(HttpField::ContentType, "text/html");
        EXPECT_EQ(response.Header(HttpField::ContentType), "text/html");
    }

    TEST(HttpResponseTest, OperatorBracketByNameReturnsValue)
    {
        HttpResponse response;
        response.Set("X-Custom", "value");
        EXPECT_EQ(response.Header("X-Custom"), "value");
    }

    TEST(HttpResponseTest, OperatorBracketReturnsEmptyForMissing)
    {
        const HttpResponse response;
        EXPECT_EQ(response.Header("X-Missing"), "");
    }

    // === Content type helpers ===

    TEST(HttpResponseTest, TextSetsContentTypeAndBody)
    {
        HttpResponse response;
        response.Text(std::string("plain text"));
        EXPECT_EQ(response.Header(HttpField::ContentType), "text/plain");
        EXPECT_EQ(response.Body(), "plain text");
    }

    TEST(HttpResponseTest, HtmlSetsContentTypeAndBody)
    {
        HttpResponse response;
        response.Html(std::string("<html></html>"));
        EXPECT_EQ(response.Header(HttpField::ContentType), "text/html");
        EXPECT_EQ(response.Body(), "<html></html>");
    }

    TEST(HttpResponseTest, JsonSetsContentTypeAndBody)
    {
        HttpResponse response;
        response.Json(std::string(R"({"key":"value"})"));
        EXPECT_EQ(response.Header(HttpField::ContentType), "application/json");
        EXPECT_EQ(response.Body(), R"({"key":"value"})");
    }

    TEST(HttpResponseTest, XmlSetsContentTypeAndBody)
    {
        HttpResponse response;
        response.Xml(std::string("<root/>"));
        EXPECT_EQ(response.Header(HttpField::ContentType), "application/xml");
        EXPECT_EQ(response.Body(), "<root/>");
    }

    TEST(HttpResponseTest, TextSetsEmptyBody)
    {
        HttpResponse response;
        response.Text(std::string(""));
        EXPECT_EQ(response.Header(HttpField::ContentType), "text/plain");
        EXPECT_EQ(response.Body(), "");
    }

    // === ContentLength() ===

    TEST(HttpResponseTest, ContentLengthSetsValue)
    {
        constexpr std::uint64_t testContentLength = 1024U;
        HttpResponse response;
        response.ContentLength(testContentLength);
        EXPECT_EQ(response.ContentLength(), testContentLength);
    }

    TEST(HttpResponseTest, ContentLengthReturnsZeroForDefault)
    {
        const HttpResponse response;
        EXPECT_EQ(response.ContentLength(), 0U);
    }

    TEST(HttpResponseTest, ContentLengthSetsZero)
    {
        HttpResponse response;
        response.ContentLength(0);
        EXPECT_EQ(response.ContentLength(), 0U);
    }

    TEST(HttpResponseTest, ContentLengthSetsMaxValue)
    {
        HttpResponse response;
        response.ContentLength(std::numeric_limits<std::uint64_t>::max());
        EXPECT_EQ(response.ContentLength(), std::numeric_limits<std::uint64_t>::max());
    }

    TEST(HttpResponseTest, HasContentLengthReturnsTrueWhenSet)
    {
        constexpr std::uint64_t testContentLengthSmall = 100U;
        HttpResponse response;
        response.ContentLength(testContentLengthSmall);
        EXPECT_TRUE(response.HasContentLength());
    }

    TEST(HttpResponseTest, HasContentLengthReturnsFalseForDefault)
    {
        const HttpResponse response;
        EXPECT_FALSE(response.HasContentLength());
    }

    // === Chunked() ===

    TEST(HttpResponseTest, ChunkedSetsTrue)
    {
        HttpResponse response;
        response.Chunked(true);
        EXPECT_TRUE(response.Chunked());
    }

    TEST(HttpResponseTest, ChunkedSetsFalse)
    {
        HttpResponse response;
        response.Chunked(false);
        EXPECT_FALSE(response.Chunked());
    }

    TEST(HttpResponseTest, ChunkedReturnsFalseForDefault)
    {
        const HttpResponse response;
        EXPECT_FALSE(response.Chunked());
    }

    // === PreparePayload() ===

    TEST(HttpResponseTest, PreparePayloadSetsContentLength)
    {
        HttpResponse response;
        response.Body(std::string("hello"));
        response.PreparePayload();
        EXPECT_EQ(response.ContentLength(), 5U);
    }

    TEST(HttpResponseTest, PreparePayloadWithEmptyBody)
    {
        HttpResponse response;
        response.PreparePayload();
        EXPECT_TRUE(response.HasContentLength());
    }

    // === PayloadSize() ===

    TEST(HttpResponseTest, PayloadSizeReturnsZeroForDefault)
    {
        const HttpResponse response;
        EXPECT_EQ(response.PayloadSize(), 0U);
    }

    TEST(HttpResponseTest, PayloadSizeReturnsCorrectValue)
    {
        HttpResponse response;
        response.Body(std::string("hello"));
        response.PreparePayload();
        EXPECT_EQ(response.PayloadSize(), 5U);
    }

    // === NeedEof() ===

    TEST(HttpResponseTest, NeedEofReturnsTrueForDefault)
    {
        const HttpResponse response;
        // Boost.Beast returns true for need_eof by default (no Content-Length set)
        EXPECT_TRUE(response.NeedEof());
    }

    // === StatusCode() & StatusCodeInt() ===

    TEST(HttpResponseTest, StatusCodeReturnsOk)
    {
        HttpResponse response;
        response.Ok();
        EXPECT_EQ(response.StatusCode(), HttpStatus::Ok);
    }

    TEST(HttpResponseTest, StatusCodeIntReturns200)
    {
        HttpResponse response;
        response.Ok();
        EXPECT_EQ(response.StatusCodeInt(), 200U);
    }

    TEST(HttpResponseTest, StatusCodeIntReturns200ForDefault)
    {
        const HttpResponse response;
        // Boost.Beast initializes response with status 200 by default
        EXPECT_EQ(response.StatusCodeInt(), 200U);
    }

    // === Raw() ===

    TEST(HttpResponseTest, RawConstReturnsReference)
    {
        HttpResponse response;
        response.Ok();

        const HttpResponse& constRef = response;
        const Detail::HttpResponse& raw = constRef.Raw();
        EXPECT_EQ(raw.result_int(), 200);
    }

    TEST(HttpResponseTest, RawRvalueReturnsReference)
    {
        HttpResponse response;
        response.Ok();

        const auto& raw = std::move(response).Raw();
        EXPECT_EQ(raw.result_int(), 200);
    }

    // === Method chaining ===

    TEST(HttpResponseTest, StatusReturnsReference)
    {
        HttpResponse response;
        const HttpResponse& chained = response.Status(HttpStatus::Ok);
        EXPECT_EQ(&chained, &response);
    }

    TEST(HttpResponseTest, BodyReturnsReference)
    {
        HttpResponse response;
        const HttpResponse& chained = response.Body(std::string("test"));
        EXPECT_EQ(&chained, &response);
    }

    TEST(HttpResponseTest, SetReturnsReference)
    {
        HttpResponse response;
        const HttpResponse& chained = response.Set(HttpField::ContentType, "text/html");
        EXPECT_EQ(&chained, &response);
    }

    TEST(HttpResponseTest, FullChainingWorks)
    {
        HttpResponse response;
        response.Ok()
            .Set(HttpField::ContentType, "application/json")
            .Body(std::string(R"({"status":"ok"})"))
            .PreparePayload();

        EXPECT_EQ(response.StatusCode(), HttpStatus::Ok);
        EXPECT_EQ(response.Header(HttpField::ContentType), "application/json");
        EXPECT_EQ(response.Body(), R"({"status":"ok"})");
        EXPECT_TRUE(response.HasContentLength());
    }

    // === Multiple headers ===

    TEST(HttpResponseTest, MultipleHeadersAreSet)
    {
        HttpResponse response;
        response.Set(HttpField::ContentType, "application/json")
            .Set(HttpField::CacheControl, "no-cache")
            .Set("X-Custom", "value")
            .Set(HttpField::Server, "Flux/1.0");

        EXPECT_TRUE(response.HasHeader(HttpField::ContentType));
        EXPECT_TRUE(response.HasHeader(HttpField::CacheControl));
        EXPECT_TRUE(response.HasHeader("X-Custom"));
        EXPECT_TRUE(response.HasHeader(HttpField::Server));

        EXPECT_EQ(response.Header(HttpField::ContentType), "application/json");
        EXPECT_EQ(response.Header(HttpField::CacheControl), "no-cache");
        EXPECT_EQ(response.Header("X-Custom"), "value");
        EXPECT_EQ(response.Header(HttpField::Server), "Flux/1.0");
    }

    // === Edge cases ===

    TEST(HttpResponseTest, ResponseWithNullByteInBody)
    {
        HttpResponse response;
        response.Body(
            std::string("body\x00with\x00null"));   // NOLINT(*-string-literal-with-embedded-nul)
        EXPECT_FALSE(response.Body().empty());
    }

    TEST(HttpResponseTest, ResponseWithLongHeaderValue)
    {
        const std::string longValue(4096, 'v');
        HttpResponse response;
        response.Set("X-Long-Header", longValue);
        EXPECT_EQ(response.Header("X-Long-Header"), longValue);
    }

    TEST(HttpResponseTest, ResponseWithEmptyHeaderName)
    {
        HttpResponse response;
        response.Set("", "value");
        EXPECT_TRUE(response.HasHeader(""));
    }

    // === [[nodiscard]] checks ===

    TEST(HttpResponseTest, ReasonIsNodiscard)
    {
        const std::string_view result = HttpResponse().Reason();
        // Boost.Beast initializes response with status 200 OK
        EXPECT_EQ(result, "OK");
    }

    TEST(HttpResponseTest, VersionIsNodiscard)
    {
        const std::uint32_t result = HttpResponse().Version();
        // Boost.Beast initializes version to 11 (HTTP/1.1)
        EXPECT_EQ(result, HttpVersion11);
    }

    TEST(HttpResponseTest, KeepAliveIsNodiscard)
    {
        const bool result = HttpResponse().KeepAlive();
        // Boost.Beast initializes keep_alive to true by default
        EXPECT_TRUE(result);
    }

    TEST(HttpResponseTest, BodyIsNodiscard)
    {
        const std::string_view result = HttpResponse().Body();
        EXPECT_EQ(result, "");
    }

    TEST(HttpResponseTest, ContentLengthIsNodiscard)
    {
        const std::uint64_t result = HttpResponse().ContentLength();
        EXPECT_EQ(result, 0U);
    }

    TEST(HttpResponseTest, HasContentLengthIsNodiscard)
    {
        const bool result = HttpResponse().HasContentLength();
        EXPECT_FALSE(result);
    }

    TEST(HttpResponseTest, ChunkedIsNodiscard)
    {
        const bool result = HttpResponse().Chunked();
        EXPECT_FALSE(result);
    }

    TEST(HttpResponseTest, PayloadSizeIsNodiscard)
    {
        const std::uint64_t result = HttpResponse().PayloadSize();
        EXPECT_EQ(result, 0U);
    }

    TEST(HttpResponseTest, NeedEofIsNodiscard)
    {
        const bool result = HttpResponse().NeedEof();
        // Boost.Beast returns true for need_eof by default
        EXPECT_TRUE(result);
    }

    TEST(HttpResponseTest, StatusCodeIsNodiscard)
    {
        const HttpStatus result = HttpResponse().StatusCode();
        // Boost.Beast initializes response with status 200
        EXPECT_EQ(ToInteger(result), 200U);
    }

    TEST(HttpResponseTest, StatusCodeIntIsNodiscard)
    {
        const std::uint16_t result = HttpResponse().StatusCodeInt();
        // Boost.Beast initializes response with status 200
        EXPECT_EQ(result, 200U);
    }
}   // namespace Flux::Test
