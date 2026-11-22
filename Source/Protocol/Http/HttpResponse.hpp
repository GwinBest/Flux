#pragma once

#include <boost/beast/http.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "Protocol/Http/HttpField.hpp"
#include "Protocol/Http/HttpStatus.hpp"

namespace Flux
{
    namespace Detail
    {
        using HttpResponse = boost::beast::http::response<boost::beast::http::string_body>;
    }   // namespace Detail

    class HttpResponse final : public Detail::HttpResponse
    {
    public:
        HttpResponse() = default;

        explicit HttpResponse(const Detail::HttpResponse& src);
        explicit HttpResponse(Detail::HttpResponse&& src);

        auto Status(const HttpStatus status) -> HttpResponse&;
        auto Status(std::uint16_t code) -> HttpResponse&;

        auto Result(const HttpStatus status) -> HttpResponse&;
        auto Result(const std::uint16_t code) -> HttpResponse&;

        auto Ok() -> HttpResponse&;
        auto Created() -> HttpResponse&;
        auto Accepted() -> HttpResponse&;
        auto NoContent() -> HttpResponse&;
        auto BadRequest() -> HttpResponse&;
        auto Unauthorized() -> HttpResponse&;
        auto Forbidden() -> HttpResponse&;
        auto NotFound() -> HttpResponse&;
        auto MethodNotAllowed() -> HttpResponse&;
        auto InternalServerError() -> HttpResponse&;
        auto NotImplemented() -> HttpResponse&;
        auto ServiceUnavailable() -> HttpResponse&;

        auto Reason(const std::string_view reason) -> HttpResponse&;
        [[nodiscard]] auto Reason() const -> std::string_view;

        auto Version(const std::uint32_t version) -> HttpResponse&;
        [[nodiscard]] auto Version() const -> std::uint32_t;

        auto KeepAlive(const bool keepAlive) -> HttpResponse&;
        [[nodiscard]] auto KeepAlive() const -> bool;

        auto Body(std::string body) -> HttpResponse&;
        auto Body(const char* body) -> HttpResponse&;
        [[nodiscard]] auto Body() const -> std::string_view;
        auto ClearBody() -> HttpResponse&;

        auto Set(const HttpField field, const std::string_view value) -> HttpResponse&;
        auto Set(const std::string_view name, const std::string_view value) -> HttpResponse&;

        auto Insert(const HttpField field, const std::string_view value) -> HttpResponse&;
        auto Insert(const std::string_view name, const std::string_view value) -> HttpResponse&;

        auto Remove(const HttpField field) -> HttpResponse&;
        auto Remove(const std::string_view name) -> HttpResponse&;

        auto Replace(const HttpField field, const std::string_view value) -> HttpResponse&;
        auto Replace(const std::string_view name, const std::string_view value) -> HttpResponse&;

        [[nodiscard]] auto Header(const HttpField field) const -> std::string_view;
        [[nodiscard]] auto Header(const std::string_view name) const -> std::string_view;

        [[nodiscard]] auto HasHeader(const HttpField field) const -> bool;
        [[nodiscard]] auto HasHeader(const std::string_view name) const -> bool;

        [[nodiscard]] auto operator[](const HttpField field) const -> std::string_view;
        [[nodiscard]] auto operator[](const std::string_view name) const -> std::string_view;

        auto Text(std::string body) -> HttpResponse&;
        auto Html(std::string body) -> HttpResponse&;
        auto Json(std::string body) -> HttpResponse&;
        auto Xml(std::string body) -> HttpResponse&;

        auto ContentLength(std::uint64_t length) -> HttpResponse&;
        [[nodiscard]] auto ContentLength() const -> std::uint64_t;
        [[nodiscard]] auto HasContentLength() const -> bool;

        auto Chunked(const bool chunked) -> HttpResponse&;
        [[nodiscard]] auto Chunked() const -> bool;

        auto PreparePayload() -> HttpResponse&;

        [[nodiscard]] auto PayloadSize() const -> std::uint64_t;
        [[nodiscard]] auto NeedEof() const -> bool;

        [[nodiscard]] auto StatusCode() const -> HttpStatus;
        [[nodiscard]] auto StatusCodeInt() const -> std::uint16_t;

        [[nodiscard]] auto Raw() const& -> const Detail::HttpResponse&;
        [[nodiscard]] auto Raw() && -> Detail::HttpResponse&;
    };
}   // namespace Flux
