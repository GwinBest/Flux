#include "HttpRequest.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace Flux
{
    HttpRequest::HttpRequest(const Detail::HttpRequest& src)
        : Detail::HttpRequest(src)
    {}

    HttpRequest::HttpRequest(Detail::HttpRequest&& src)
        : Detail::HttpRequest(std::move(src))
    {}

    auto HttpRequest::Path() const -> std::string_view
    {
        return target();
    }

    auto HttpRequest::Method() const -> HttpMethod
    {
        return FromString(boost::beast::http::to_string(method()));
    }

    auto HttpRequest::Version() const -> std::uint32_t
    {
        return Detail::HttpRequest::version();
    }

    auto HttpRequest::KeepAlive() const -> bool
    {
        return Detail::HttpRequest::keep_alive();
    }

    auto HttpRequest::Body() const -> std::string_view
    {
        return Detail::HttpRequest::body();
    }

    auto HttpRequest::PayloadSize() const -> std::uint64_t
    {
        const auto opt = Detail::HttpRequest::payload_size();
        return opt ? static_cast<std::uint64_t>(*opt) : 0;
    }

    auto HttpRequest::Header(const HttpField field) const -> std::string_view
    {
        return Detail::HttpRequest::   // NOLINT(*-pro-bounds-avoid-unchecked-container-access)
            operator[](ToString(field));
    }

    auto HttpRequest::Header(const std::string_view name) const -> std::string_view
    {
        return Detail::HttpRequest::   // NOLINT(*-pro-bounds-avoid-unchecked-container-access)
            operator[](std::string(name));
    }

    auto HttpRequest::HasHeader(const HttpField field) const -> bool
    {
        return Detail::HttpRequest::contains(ToString(field));
    }

    auto HttpRequest::HasHeader(const std::string_view name) const -> bool
    {
        return Detail::HttpRequest::contains(std::string(name));
    }

    auto HttpRequest::operator[](const HttpField field) const -> std::string_view
    {
        return Header(field);
    }

    auto HttpRequest::operator[](const std::string_view name) const -> std::string_view
    {
        return Header(name);
    }

    auto HttpRequest::Raw() const& -> const Detail::HttpRequest&
    {
        return *this;
    }

    auto HttpRequest::Raw() && -> Detail::HttpRequest&
    {
        return *this;
    }
}   // namespace Flux
