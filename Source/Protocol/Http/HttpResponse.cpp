#include "HttpResponse.hpp"

namespace Flux
{
    HttpResponse::HttpResponse(const Detail::HttpResponse& src)
        : Detail::HttpResponse(src)
    {}

    HttpResponse::HttpResponse(Detail::HttpResponse&& src)
        : Detail::HttpResponse(std::move(src))
    {}

    auto HttpResponse::Status(const HttpStatus status) -> HttpResponse&
    {
        Detail::HttpResponse::result(static_cast<boost::beast::http::status>(ToInteger(status)));
        return *this;
    }

    auto HttpResponse::Status(std::uint16_t code) -> HttpResponse&
    {
        Detail::HttpResponse::result(static_cast<boost::beast::http::status>(code));
        return *this;
    }

    auto HttpResponse::Result(const HttpStatus status) -> HttpResponse&
    {
        return Status(status);
    }

    auto HttpResponse::Result(const std::uint16_t code) -> HttpResponse&
    {
        return Status(code);
    }

    auto HttpResponse::Ok() -> HttpResponse&
    {
        return Status(HttpStatus::Ok);
    }

    auto HttpResponse::Created() -> HttpResponse&
    {
        return Status(HttpStatus::Created);
    }

    auto HttpResponse::Accepted() -> HttpResponse&
    {
        return Status(HttpStatus::Accepted);
    }

    auto HttpResponse::NoContent() -> HttpResponse&
    {
        return Status(HttpStatus::NoContent);
    }

    auto HttpResponse::BadRequest() -> HttpResponse&
    {
        return Status(HttpStatus::BadRequest);
    }

    auto HttpResponse::Unauthorized() -> HttpResponse&
    {
        return Status(HttpStatus::Unauthorized);
    }

    auto HttpResponse::Forbidden() -> HttpResponse&
    {
        return Status(HttpStatus::Forbidden);
    }

    auto HttpResponse::NotFound() -> HttpResponse&
    {
        return Status(HttpStatus::NotFound);
    }

    auto HttpResponse::MethodNotAllowed() -> HttpResponse&
    {
        return Status(HttpStatus::MethodNotAllowed);
    }

    auto HttpResponse::InternalServerError() -> HttpResponse&
    {
        return Status(HttpStatus::InternalServerError);
    }

    auto HttpResponse::NotImplemented() -> HttpResponse&
    {
        return Status(HttpStatus::NotImplemented);
    }

    auto HttpResponse::ServiceUnavailable() -> HttpResponse&
    {
        return Status(HttpStatus::ServiceUnavailable);
    }

    auto HttpResponse::Reason(const std::string_view reason) -> HttpResponse&
    {
        Detail::HttpResponse::reason(reason);
        return *this;
    }

    auto HttpResponse::Reason() const -> std::string_view
    {
        return Detail::HttpResponse::reason();
    }

    auto HttpResponse::Version(const std::uint32_t version) -> HttpResponse&
    {
        Detail::HttpResponse::version(version);
        return *this;
    }

    auto HttpResponse::Version() const -> std::uint32_t
    {
        return Detail::HttpResponse::version();
    }

    auto HttpResponse::KeepAlive(const bool keepAlive) -> HttpResponse&
    {
        Detail::HttpResponse::keep_alive(keepAlive);
        return *this;
    }

    auto HttpResponse::KeepAlive() const -> bool
    {
        return Detail::HttpResponse::keep_alive();
    }

    auto HttpResponse::Body(std::string body) -> HttpResponse&
    {
        Detail::HttpResponse::body() = std::move(body);
        return *this;
    }

    auto HttpResponse::Body(const char* body) -> HttpResponse&
    {
        Detail::HttpResponse::body() = body;
        return *this;
    }

    auto HttpResponse::Body() const -> std::string_view
    {
        return Detail::HttpResponse::body();
    }

    auto HttpResponse::ClearBody() -> HttpResponse&
    {
        Detail::HttpResponse::body().clear();
        return *this;
    }

    auto HttpResponse::Set(const HttpField field, const std::string_view value) -> HttpResponse&
    {
        Detail::HttpResponse::set(ToString(field), value);
        return *this;
    }

    auto HttpResponse::Set(const std::string_view name, const std::string_view value)
        -> HttpResponse&
    {
        Detail::HttpResponse::set(std::string(name), value);
        return *this;
    }

    auto HttpResponse::Insert(const HttpField field, const std::string_view value) -> HttpResponse&
    {
        Detail::HttpResponse::insert(ToString(field), value);
        return *this;
    }

    auto HttpResponse::Insert(const std::string_view name, const std::string_view value)
        -> HttpResponse&
    {
        Detail::HttpResponse::insert(std::string(name), value);
        return *this;
    }

    auto HttpResponse::Remove(const HttpField field) -> HttpResponse&
    {
        Detail::HttpResponse::base().erase(ToString(field));
        return *this;
    }

    auto HttpResponse::Remove(const std::string_view name) -> HttpResponse&
    {
        Detail::HttpResponse::base().erase(std::string(name));
        return *this;
    }

    auto HttpResponse::Replace(const HttpField field, const std::string_view value) -> HttpResponse&
    {
        Detail::HttpResponse::set(ToString(field), value);
        return *this;
    }

    auto HttpResponse::Replace(const std::string_view name, const std::string_view value)
        -> HttpResponse&
    {
        Detail::HttpResponse::set(std::string(name), value);
        return *this;
    }

    auto HttpResponse::Header(const HttpField field) const -> std::string_view
    {
        return Detail::HttpResponse::   // NOLINT(*-pro-bounds-avoid-unchecked-container-access)
            operator[](ToString(field));
    }

    auto HttpResponse::Header(const std::string_view name) const -> std::string_view
    {
        return Detail::HttpResponse::   // NOLINT(*-pro-bounds-avoid-unchecked-container-access)
            operator[](std::string(name));
    }

    auto HttpResponse::HasHeader(const HttpField field) const -> bool
    {
        return Detail::HttpResponse::contains(ToString(field));
    }

    auto HttpResponse::HasHeader(const std::string_view name) const -> bool
    {
        return Detail::HttpResponse::contains(std::string(name));
    }

    auto HttpResponse::operator[](const HttpField field) const -> std::string_view
    {
        return Header(field);
    }

    auto HttpResponse::operator[](const std::string_view name) const -> std::string_view
    {
        return Header(name);
    }

    auto HttpResponse::Text(std::string body) -> HttpResponse&
    {
        Set(HttpField::ContentType, "text/plain");
        Body(std::move(body));
        Detail::HttpResponse::prepare_payload();
        return *this;
    }

    auto HttpResponse::Html(std::string body) -> HttpResponse&
    {
        Set(HttpField::ContentType, "text/html");
        Body(std::move(body));
        Detail::HttpResponse::prepare_payload();
        return *this;
    }

    auto HttpResponse::Json(std::string body) -> HttpResponse&
    {
        Set(HttpField::ContentType, "application/json");
        Body(std::move(body));
        Detail::HttpResponse::prepare_payload();
        return *this;
    }

    auto HttpResponse::Xml(std::string body) -> HttpResponse&
    {
        Set(HttpField::ContentType, "application/xml");
        Body(std::move(body));
        Detail::HttpResponse::prepare_payload();
        return *this;
    }

    auto HttpResponse::ContentLength(std::uint64_t length) -> HttpResponse&
    {
        Detail::HttpResponse::content_length(length);
        return *this;
    }

    auto HttpResponse::ContentLength() const -> std::uint64_t
    {
        const auto it = Detail::HttpResponse::base().find(   // NOLINT(*-identifier-length)
            boost::beast::http::field::content_length);
        if (it != Detail::HttpResponse::base().end())
        {
            return std::stoull(std::string(it->value()));
        }
        return 0;
    }

    auto HttpResponse::HasContentLength() const -> bool
    {
        return Detail::HttpResponse::base().contains(boost::beast::http::field::content_length);
    }

    auto HttpResponse::Chunked(const bool chunked) -> HttpResponse&
    {
        Detail::HttpResponse::chunked(chunked);
        return *this;
    }

    auto HttpResponse::Chunked() const -> bool
    {
        return Detail::HttpResponse::chunked();
    }

    auto HttpResponse::PreparePayload() -> HttpResponse&
    {
        Detail::HttpResponse::prepare_payload();
        return *this;
    }

    auto HttpResponse::PayloadSize() const -> std::uint64_t
    {
        const auto opt = Detail::HttpResponse::payload_size();
        return opt ? static_cast<std::uint64_t>(*opt) : 0;
    }

    auto HttpResponse::NeedEof() const -> bool
    {
        return Detail::HttpResponse::need_eof();
    }

    auto HttpResponse::StatusCode() const -> HttpStatus
    {
        return static_cast<HttpStatus>(
            static_cast<std::uint16_t>(Detail::HttpResponse::result_int()));
    }

    auto HttpResponse::StatusCodeInt() const -> std::uint16_t
    {
        return static_cast<std::uint16_t>(Detail::HttpResponse::result_int());
    }

    auto HttpResponse::Raw() const& -> const Detail::HttpResponse&
    {
        return *this;
    }

    auto HttpResponse::Raw() && -> Detail::HttpResponse&
    {
        return *this;
    }
}   // namespace Flux
