#pragma once

#include <boost/beast/http.hpp>
#include <cstdint>
#include <string_view>

#include "Protocol/Http/HttpField.hpp"
#include "Protocol/Http/HttpMethod.hpp"

namespace Flux
{
    namespace Detail
    {
        using HttpRequest = boost::beast::http::request<boost::beast::http::string_body>;
    }   // namespace Detail

    class HttpRequest final : public Detail::HttpRequest
    {
    public:
        HttpRequest() = default;

        explicit HttpRequest(const Detail::HttpRequest& src);
        explicit HttpRequest(Detail::HttpRequest&& src);

        [[nodiscard]] auto Path() const -> std::string_view;
        [[nodiscard]] auto Method() const -> HttpMethod;
        [[nodiscard]] auto Version() const -> std::uint32_t;
        [[nodiscard]] auto KeepAlive() const -> bool;
        [[nodiscard]] auto Body() const -> std::string_view;
        [[nodiscard]] auto PayloadSize() const -> std::uint64_t;

        [[nodiscard]] auto Header(const HttpField field) const -> std::string_view;
        [[nodiscard]] auto Header(const std::string_view name) const -> std::string_view;

        [[nodiscard]] auto HasHeader(const HttpField field) const -> bool;
        [[nodiscard]] auto HasHeader(const std::string_view name) const -> bool;

        [[nodiscard]] auto operator[](const HttpField field) const -> std::string_view;
        [[nodiscard]] auto operator[](const std::string_view name) const -> std::string_view;

        [[nodiscard]] auto Raw() const& -> const Detail::HttpRequest&;
        [[nodiscard]] auto Raw() && -> Detail::HttpRequest&;
    };
}   // namespace Flux
